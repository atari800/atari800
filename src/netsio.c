/*
* netsio.c - NetSIO interface for FujiNet-PC <-> Atari800 Emulator
*
* Uses two threads:
*  - fujinet_rx_thread: receive from FujiNet-PC, respond to pings/alives, queue complete packets to emulator
*  - emu_tx_thread: receive from emulator FIFO, queue complete packets to FujiNet-PC
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include "netsio.h"
#include "log.h"
#include "pia.h" /* For toggling PROC & INT */
#if defined(__APPLE__) && defined(__MACH__)
#include "SDL.h" /* For SDL_Delay() on macOS */
#else
#include "SDL/SDL.h" /* For SDL_Delay() on Linux */
#endif

/* Flag to know when netsio is enabled */
volatile int netsio_enabled = 0;
/* Holds sync to fujinet-pc incremented number */
uint8_t netsio_sync_num = 0;
/* if we have heard from fujinet-pc or not */
int fujinet_known = 0;
/* wait for fujinet sync if true */
volatile int netsio_sync_wait = 0;
/* true if cmd line pulled */
int netsio_cmd_state = 0;
/* data frame size for SIO write commands */
volatile int netsio_next_write_size = 0;

/* FIFO pipes:
* fds0: FujiNet->emulator
* fds1: emulator->FujiNet
*/
int fds0[2], fds1[2];

/* UDP socket for NetSIO and return address holder */
static int sockfd = -1;
static struct sockaddr_storage fujinet_addr;
static socklen_t fujinet_addr_len = sizeof(fujinet_addr);

/* Thread declarations */
static void *fujinet_rx_thread(void *arg);
static void *emu_tx_thread(void *arg);

char *buf_to_hex(const uint8_t *buf, size_t offset, size_t len) {
    /* each byte takes "XX " == 3 chars, +1 for trailing NUL */
    size_t needed = len * 3 + 1;
    char *s = malloc(needed);
    size_t i = 0;
    if (!s) return NULL;
    char *p = s;
    for (i = 0; i < len; i++) {
        sprintf(p, "%02X ", buf[offset + i]);
        p += 3;
    }
    if (len) {
        p[-1] = '\0';
    } else {
        *p = '\0';
    }
    return s;
}

/* write data to emulator FIFO (fujinet_rx_thread) */
static void enqueue_to_emulator(const uint8_t *pkt, size_t len) {
    ssize_t n;
    while (len > 0) {
        n = write(fds0[1], pkt, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("netsio: write to emulator FIFO");
            /*exit(1);*/
        }
        pkt += n;
        len -= n;
    }
}

/* send a packet to FujiNet socket */
static void send_to_fujinet(const uint8_t *pkt, size_t len) {
    ssize_t n;

    /* if we never received a ping from FujiNet or we have no address to reply to */
    if (!fujinet_known || fujinet_addr.ss_family != AF_INET) {
        Log_print("netsio: can't send_to_fujinet, no address");
        return;
    }
    
    /*
     * PLATFORM SPECIFIC: BSD Socket API Difference
     * macOS/BSD: Requires exact address structure size for socket operations
     * Linux: More forgiving, accepts larger-than-necessary address length
     *
     * Using the correct size for IPv4 addresses ensures compatibility with both Linux and macOS
     */
    socklen_t addr_len = sizeof(struct sockaddr_in);
    
    /*
     * PLATFORM SPECIFIC: Socket flags handling
     * MSG_NOSIGNAL is defined but not supported on macOS (will cause EINVAL)
     * Linux uses MSG_NOSIGNAL to prevent SIGPIPE when the connection is closed
     * 
     * On macOS, SIGPIPE is typically handled using the SO_NOSIGPIPE socket option
     * instead, but we're just avoiding the flag entirely for simplicity
     */
    int flags = 0;
#if defined(MSG_NOSIGNAL) && !defined(__APPLE__)
    /* Only use MSG_NOSIGNAL on Linux and other platforms that support it */
    flags |= MSG_NOSIGNAL;
#endif

    n = sendto(
        sockfd,
        pkt, len, flags,
        (struct sockaddr *)&fujinet_addr,
        addr_len
    );
    if (n < 0) {
        if (errno == EINTR) {
            /* transient, try once more */
            n = sendto(
                sockfd,
                pkt, len, flags,
                (struct sockaddr *)&fujinet_addr,
                addr_len
            );
        }
        if (n < 0) {
            char errmsg[256];
            snprintf(errmsg, sizeof(errmsg), "netsio: sendto FujiNet failed with errno %d: %s", errno, strerror(errno));
            Log_print("%s", errmsg);
            return;
        }
    } else if ((size_t)n != len) {
        Log_print("netsio: partial send (%zd of %zu bytes)", n, len);
        return;
    }

    /* build a hex string: each byte "XX " */
    size_t buf_size = len * 3 + 1;
    char hexdump[buf_size];
    size_t pos = 0;
    size_t i = 0;
    for (i = 0; i < len; i++) {
        /* snprintf returns number of chars (excluding trailing NUL) */
        int written = snprintf(&hexdump[pos], buf_size - pos, "%02X ", pkt[i]);
        if (written < 0 || (size_t)written >= buf_size - pos) {
            break;
        }
        pos += written;
    }
    hexdump[pos] = '\0';
    /* Log_print("netsio: send: %zu bytes → %s", len, hexdump); */
}


/* Send a single byte as a DATA_BYTE packet */
void send_byte_to_fujinet(uint8_t data_byte) {
    uint8_t packet[2];
    packet[0] = NETSIO_DATA_BYTE;
    packet[1] = data_byte;
    send_to_fujinet(packet, sizeof(packet));
}

/* Send up to 512 bytes as a DATA_BLOCK packet */
void send_block_to_fujinet(const uint8_t *block, size_t len) {
    if (len == 0 || len > 512) return;  /* sanity check */

    uint8_t packet[512 + 2];
    packet[0] = NETSIO_DATA_BLOCK;
    memcpy(&packet[1], block, len);
    /* Pad the end with a junk byte or FN-PC won't accept the packet */
    packet[1 + len] = 0xFF;
    send_to_fujinet(packet, len + 2);
}

/* Initialize NetSIO:
*   - connect to FujiNet socket
*   - create FIFOs
*   - spawn the two threads
*/
int netsio_init(uint16_t port) {
    struct sockaddr_in addr;
    pthread_t rx_thread, tx_thread;

    /* create emulator <-> netsio FIFOs */
    if (pipe(fds0) < 0 || pipe(fds1) < 0) {
        perror("netsio: pipe");
        return -1;
    }
    /* fds0[0] = emulator reads here (FujiNet->emu)
    fds0[1] = netsio_rx_thread writes here */
    /* fds1[0] = netsio_tx_thread reads here
    fds1[1] = emulator writes here (emu->FujiNet) */

    /* connect socket to FujiNet */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("netsio: socket");
        return -1;
    }
    /* Fill in the structure with port number, any IP */
    memset(&addr, 0, sizeof(addr));
    
    /*
     * PLATFORM SPECIFIC: macOS/BSD Socket API Difference
     * BSD sockets (including macOS) require sin_len field to be set
     * Linux ignores this field since it doesn't exist in the Linux socket API
     */
#ifdef __APPLE__
    addr.sin_len = sizeof(addr); /* Only needed on macOS/BSD systems */
#endif
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    /* 
     * Enable broadcast on the socket - required on all platforms
     * This is needed for sending broadcast packets to FujiNet
     * Works the same way on both Linux and macOS
     */
    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("netsio setsockopt SO_BROADCAST");
    }

    /* Bind to the socket on requested port */
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("netsio bind");
        close(sockfd);
        return -1;
    }

    /* spawn receiver thread */
    if (pthread_create(&rx_thread, NULL, fujinet_rx_thread, NULL) != 0) {
        perror("netsio: pthread_create rx");
        return -1;
    }
    pthread_detach(rx_thread);

    /* spawn transmitter thread */
/* Disabled this thread
    if (pthread_create(&tx_thread, NULL, emu_tx_thread, NULL) != 0) {
        perror("netsio: pthread_create tx");
        return -1;
    }
    pthread_detach(tx_thread);
*/
    return 0;
}

/* Called when a command frame with sync response is sent to FujiNet */
void netsio_wait_for_sync(void)
{
    int ticker = 0;
    while (netsio_sync_wait) {
        Log_print("netsio: waiting for sync response - %d", ticker);
        SDL_Delay(5);
        if (ticker > 7)
            break;
        ticker++;
    }
}

/* Return number of bytes waiting from FujiNet to emulator */
int netsio_available(void) {
    int avail = 0;
    if (fds0[0] >= 0) {
        if (ioctl(fds0[0], FIONREAD, &avail) < 0) {
                Log_print("netsio_avail: ioctl error");
                return -1;
        }
    }
    return avail;
}

/* COMMAND ON */
int netsio_cmd_on(void)
{
    Log_print("netsio: CMD ON");
    netsio_cmd_state = 1;
    uint8_t p = NETSIO_COMMAND_ON;
    send_to_fujinet(&p, 1);
    return 0;
}

/* COMMAND OFF */
int netsio_cmd_off(void)
{
    Log_print("netsio: CMD OFF");
    uint8_t p = NETSIO_COMMAND_OFF;
    send_to_fujinet(&p, 1);
    return 0;
}

/* COMMAND OFF with SYNC */
int netsio_cmd_off_sync(void)
{
    Log_print("netsio: CMD OFF SYNC");
    netsio_sync_num++;
    uint8_t p[2] = { NETSIO_COMMAND_OFF_SYNC, netsio_sync_num };
    send_to_fujinet(&p, sizeof(p));
    netsio_sync_wait = 1; /* pause emulation til we hear back */
    return 0;
}

/* Toggle Command Line */
void netsio_toggle_cmd(int v)
{
    if (!v)
        netsio_cmd_off_sync();
    else
        netsio_cmd_on();
}

/* The emulator calls this to send a data byte out to FujiNet */
int netsio_send_byte(uint8_t b) {
    uint8_t pkt[2] = { NETSIO_DATA_BYTE, b };
    Log_print("netsio: send byte: %02X", b);
    send_to_fujinet(&pkt, 2);
    return 0;
}

/* The emulator calls this to send a data block out to FujiNet */
int netsio_send_block(const uint8_t *block, ssize_t len) {
    /* ssize_t len = sizeof(block);*/ 
    send_block_to_fujinet(block, len);
    Log_print("netsio: send block, %i bytes:\n  %s", len, buf_to_hex(block, 0, len));
}

/* DATA BYTE with SYNC */
int netsio_send_byte_sync(uint8_t b)
{
    netsio_sync_num++;
    Log_print("netsio: send byte: 0x%02X sync: %d", b, netsio_sync_num);
    uint8_t p[3] = { NETSIO_DATA_BYTE_SYNC, b, netsio_sync_num};
    send_to_fujinet(&p, sizeof(p));
    netsio_sync_wait = 1; /* pause emulation til we hear back */
    return 0;
}

/* The emulator calls this to receive a data byte from FujiNet */
int netsio_recv_byte(uint8_t *b) {
    ssize_t n = read(fds0[0], b, 1);
    if (n < 0) {
        if (errno == EINTR) return netsio_recv_byte(b);
        perror("netsio: read from rx FIFO");
        return -1;
    }
    if (n == 0) {
        /* FIFO closed? */
        return -1;
    }
#ifdef DEBUG2
    Log_print("netsio: read to emu: %02X", (unsigned)*b);
#endif
    return 0;
}

/* Send netsio COLD reset 0xFF */
int netsio_cold_reset(void) {
    uint8_t pkt = 0xFF;
    Log_print("netsio: cold reset");
    send_to_fujinet(&pkt, 1);
    return 0;
}

/* Send netsio WARM reset 0xFE */
int netsio_warm_reset(void) {
    uint8_t pkt = 0xFE;
    Log_print("netsio: warm reset");
    send_to_fujinet(&pkt, 1);
    return 0;
}

/* Send a test command frame to fujinet-pc */
void netsio_test_cmd(void)
{
    uint8_t p[6] = { 0x70, 0xE8, 0x00, 0x00, 0x59 }; /* Send fujidev get adapter config request */
    netsio_cmd_on(); /* Turn on CMD */
    send_block_to_fujinet(p, sizeof(p));
    /* send_byte_to_fujinet(0x70);
    send_byte_to_fujinet(0xE8);
    send_byte_to_fujinet(0x00);
    send_byte_to_fujinet(0x00);
    send_byte_to_fujinet(0x59); */
    netsio_cmd_off_sync(); /* Turn off CMD */
}

/* Thread: receive from FujiNet socket (one packet == one command) */
static void *fujinet_rx_thread(void *arg) {
    uint8_t buf[4096];
    uint8_t packet[65536];

    for (;;) {
        /* 
         * Always initialize with full sockaddr_storage size for receiving
         * This works on both Linux and macOS - we need full size for first connect
         */
        fujinet_addr_len = sizeof(fujinet_addr);
        
        /*
         * PLATFORM SPECIFIC: BSD Socket API Difference
         * macOS/BSD: The sin_len field must be set to the size of the address structure
         * Linux: Does not have or use the sin_len field
         *
         * If we've already established communication and know it's IPv4, we set the
         * appropriate length in the sin_len field to ensure proper socket operation on macOS.
         */
#ifdef __APPLE__
        if (fujinet_addr.ss_family == AF_INET) {
            /* Only on macOS/BSD: Set sin_len for existing IPv4 connection */
            struct sockaddr_in *addr_in = (struct sockaddr_in *)&fujinet_addr;
            addr_in->sin_len = sizeof(struct sockaddr_in);
        }
#endif
        
        ssize_t n = recvfrom(sockfd,
                             buf, sizeof(buf),
                             0,
                             (struct sockaddr *)&fujinet_addr,
                             &fujinet_addr_len);
        if (n <= 0) {
            perror("netsio: recv");
            continue;
        }
        fujinet_known = 1;
        
        /* Update the address length to the correct size for future sends */
        if (fujinet_addr.ss_family == AF_INET) {
            /* For IPv4, use sizeof sockaddr_in */
            fujinet_addr_len = sizeof(struct sockaddr_in);
        }

        /* Every packet must be at least one byte (the command) */
        if (n < 1) {
            Log_print("netsio: empty packet");
            continue;
        }

        uint8_t cmd = buf[0];

        switch (cmd) {
            case NETSIO_PING_REQUEST: {
                uint8_t r = NETSIO_PING_RESPONSE;
                send_to_fujinet(&r, 1);
                Log_print("netsio: recv: PING→PONG");
                break;
            }

            case NETSIO_DEVICE_CONNECTED: {
                Log_print("netsio: recv: device connected");
                /* give it some credits 
                uint8_t reply[2] = { NETSIO_CREDIT_UPDATE, 3 };
                send_to_fujinet(reply, sizeof(reply)); */
                netsio_enabled = 1;
                break;
            }

            case NETSIO_DEVICE_DISCONNECTED: {
                Log_print("netsio: recv: device disconnected");
                netsio_enabled = 0;
                break;
            }
            
            case NETSIO_ALIVE_REQUEST: {
                uint8_t r = NETSIO_ALIVE_RESPONSE;
                send_to_fujinet(&r, 1);
                /* Log_print("netsio: recv: IT'S ALIVE!"); */
                break;
            }

            case NETSIO_CREDIT_STATUS: {
                /* packet should be 2 bytes long */
                if (n < 2) {
                    Log_print("netsio: recv: CREDIT_STATUS packet too short (%zd)", n);
                }
                uint8_t reply[2] = { NETSIO_CREDIT_UPDATE, 3 };
                send_to_fujinet(reply, sizeof(reply));
                Log_print("netsio: recv: credit status & response");
                break;
            }

            case NETSIO_SPEED_CHANGE: {
                /* packet: [cmd][baud32le] */
                if (n < 5) {
                    Log_print("netsio: recv: SPEED_CHANGE packet too short (%zd)", n);
                    break;
                }
                uint32_t baud = buf[1]
                              | (uint32_t)buf[2] << 8
                              | (uint32_t)buf[3] << 16
                              | (uint32_t)buf[4] << 24;
                Log_print("netsio: recv: requested baud rate %u", baud);
                /* TODO: apply baud */
                send_to_fujinet(buf, 5); /* echo back */
                break;
            }

            case NETSIO_SYNC_RESPONSE: {
                /* packet: [cmd][sync#][ack_type][ack_byte][write_lo][write_hi] */
                if (n < 6) {
                    Log_print("netsio: recv: SYNC_RESPONSE too short (%zd)", n);
                    break;
                }
                uint8_t  resp_sync  = buf[1];
                uint8_t  ack_type   = buf[2];
                uint8_t  ack_byte   = buf[3];
                uint16_t write_size = buf[4] | (uint16_t)buf[5] << 8;

                if (resp_sync != netsio_sync_num) {
                    Log_print("netsio: recv: sync-response: got %u, want %u",
                              resp_sync, netsio_sync_num);
                } else {
                    if (ack_type == 0) {
                        Log_print("netsio: recv: sync %u NAK, dropping", resp_sync);
                    } else if (ack_type == 1) {
                        netsio_next_write_size = write_size;
                        Log_print("netsio: recv: sync %u ACK byte=0x%02X  write_size=0x%04X",
                                  resp_sync, ack_byte, write_size);
                        enqueue_to_emulator(&ack_byte, 1);
                    } else {
                        Log_print("netsio: recv: sync %u unknown ack_type %u",
                                  resp_sync, ack_type);
                    }
                }
                netsio_sync_wait = 0; /* continue emulation */
                break;
            }

            /* set_CA1 */
            case NETSIO_PROCEED_ON: {

                break;
            }
            case NETSIO_PROCEED_OFF: {

                break;
            }

            /* set_CB1 */
            case NETSIO_INTERRUPT_ON: {

                break;
            }
            case NETSIO_INTERRUPT_OFF: {

                break;
            }
            case NETSIO_DATA_BYTE: {
                /* packet: [cmd][data] */
                if (n < 2) {
                    Log_print("netsio: recv: DATA_BYTE too short (%zd)", n);
                    break;
                }
                uint8_t data = buf[1];
                Log_print("netsio: recv: data byte: 0x%02X", data);
                enqueue_to_emulator(&data, 1);
                break;
            }

            case NETSIO_DATA_BLOCK: {
                /* packet: [cmd][payload...] */
                if (n < 2) {
                    Log_print("netsio: recv: data block too short (%zd)", n);
                    break;
                }
                /* payload length is everything after the command byte */
                size_t payload_len = n - 1;
                Log_print("netsio: recv: data block %zu bytes:\n  %s", payload_len, buf_to_hex(buf, 1, payload_len));
                /* forward only buf[1]..buf[n-1] */
                enqueue_to_emulator(buf + 1, payload_len);
                break;
            }            

            default:
                Log_print("netsio: recv: unknown cmd 0x%02X, length %zd", cmd, n);
                break;
        }
    }
    return NULL;
}

/* Thread: receive from emulator FIFO and send to FujiNet socket, disabled/not used now */
static void *emu_tx_thread(void *arg) {
    uint8_t buf[4096];
    size_t head = 0, tail = 0;
    uint8_t packet[65536];
    int i;

    for (;;) {
        ssize_t n = read(fds1[0], buf + tail, sizeof(buf) - tail);
        if (n <= 0) {
            perror("netsio: read from TX FIFO");
            /*exit(1);**/
        }
        tail += n;

        head = 0;
        while (head < tail) {
            uint8_t cmd = buf[head];
            size_t rem = tail - head;

            /* Handle COMMAND ON */
            if (cmd == NETSIO_COMMAND_ON) {
                uint8_t r = NETSIO_COMMAND_ON;
                Log_print("netsio: CMD ON");
                send_to_fujinet(&r, 1);
                head++;
                continue;
            }

            /* Handle COMMAND OFF */
            if (cmd == NETSIO_COMMAND_OFF) {
                uint8_t r = NETSIO_COMMAND_OFF;
                Log_print("netsio: CMD OFF");
                send_to_fujinet(&r, 1);
                head++;
                continue;
            }

            /* Handle COMMAND OFF SYNC */
            if (cmd == NETSIO_COMMAND_OFF_SYNC) {
                uint8_t r = NETSIO_COMMAND_OFF_SYNC;
                uint8_t b = 0x01;
                Log_print("netsio: CMD OFF SYNC");
                send_to_fujinet(&r, 1);
                send_to_fujinet(&b, 1); /* FIXME: send real incremented sync counter */
                head++;
                continue;
            }

            /* Handle other NETSIO frames */
            size_t pkt_len = 1;
            if ((cmd == NETSIO_DATA_BYTE) || (cmd == NETSIO_DATA_BYTE_SYNC)) {
                if (rem < 2) break;
                pkt_len = 2;
            } else if (cmd == NETSIO_DATA_BLOCK) {
                if (rem < 3) break;
                uint16_t L = buf[head+1] | (buf[head+2] << 8);
                if (rem < 3 + L) break;
                pkt_len = 3 + L;
            }

            memcpy(packet, buf + head, pkt_len);
            send_to_fujinet(packet, pkt_len);
            head += pkt_len;
        }

        if (head) {
            memmove(buf, buf + head, tail - head);
            tail -= head;
        }
    }
    return NULL;
}