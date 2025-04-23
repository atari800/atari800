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
#include <netinet/in.h>
#include "netsio.h"
#include "log.h"

/* Flag to know when netsio is enabled */
int netsio_enabled = 0;
/* Holds sync to fujinet-pc incremented number */
uint8_t netsio_sync_num = 0;
/* if we have heard from fujinet-pc or not */
int fujinet_known = 0;

/* FIFO pipes:
* fds0: FujiNet->emulator
* fds1: emulator->FujiNet
*/
static int fds0[2], fds1[2];

/* UDP socket for NetSIO and return address holder */
static int sockfd = -1;
static struct sockaddr_storage fujinet_addr;
static socklen_t fujinet_addr_len = sizeof(fujinet_addr);

/* Thread declarations */
static void *fujinet_rx_thread(void *arg);
static void *emu_tx_thread(void *arg);

/* write a full packet to emulator FIFO (fujinet_rx_thread) */
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
    if (fujinet_known == 0 || fujinet_addr.ss_family != AF_INET) {
        /* skip sending */
        return;
    }

    while (len > 0) {
        n = sendto(sockfd, pkt, len, 0,
                   (struct sockaddr *)&fujinet_addr, fujinet_addr_len);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("netsio: send to FujiNet");
            /*exit(1);*/
        }
        pkt  += n;
        len  -= n;
    }
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
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    /*
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("netsio: connect");
        close(sockfd);
        return -1;
    }*/

    /* Bind to the socket on requested port */
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("netsio bind");
    close(sockfd);
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

/* COMMAND ON */
int netsio_cmd_on(void)
{
    Log_print("netsio: CMD ON");
    netsio_send_byte(NETSIO_COMMAND_ON);
    return 0;

}

/* COMMAND OFF */
int netsio_cmd_off(void)
{
    Log_print("netsio: CMD OFF");
    netsio_send_byte(NETSIO_COMMAND_OFF);
    return 0;

}

/* COMMAND OFF with SYNC */
int netsio_cmd_off_sync(void)
{
    Log_print("netsio: CMD OFF SYNC");
    netsio_send_byte(NETSIO_COMMAND_OFF_SYNC);
    netsio_send_byte(netsio_sync_num);
    netsio_sync_num++;
    return 0;

}

/* The emulator calls this to send a byte out to FujiNet */
int netsio_send_byte(uint8_t b) {
    send_to_fujinet(&b, sizeof(b));  
    return 0;
}

/* The emulator calls this to receive a byte from FujiNet */
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
    return 0;
}

/* Thread: receive from FujiNet socket */
static void *fujinet_rx_thread(void *arg) {
    uint8_t  buf[4096];
    size_t   head = 0, tail = 0;
    uint8_t  packet[65536];

    for (;;) {
        fujinet_addr_len = sizeof(fujinet_addr);
        ssize_t n = recvfrom(sockfd, buf + tail, sizeof(buf) - tail, 0,
                             (struct sockaddr *)&fujinet_addr,
                             &fujinet_addr_len);
        if (n <= 0) {
            perror("netsio: recv");
            /*exit(1);*/
        }
        
        fujinet_known = 1;

        tail += n;

        /* process as many complete packets as possible */
        head = 0;
        while (head < tail) {
            uint8_t cmd = buf[head];
            size_t  remaining = tail - head;

            /* PING request and immediate response */
            if (cmd == NETSIO_PING_REQUEST) {
                uint8_t r = NETSIO_PING_RESPONSE;
                send_to_fujinet(&r, 1);
                Log_print("netsio: PING->PONG");
                netsio_enabled = 1;
                head++;
                continue;
            }

            /* Alive request and immediate response */
            if (cmd == NETSIO_ALIVE_REQUEST) {
                uint8_t r = NETSIO_ALIVE_RESPONSE;
                send_to_fujinet(&r, 1);
                head++;
                continue;
            }
            /* SPEED_CHANGE: 1 + 4 bytes of baud */
            if (cmd == NETSIO_SPEED_CHANGE) {
                if (remaining < 5) break;
                /* parse little-endian 32-bit baud */
                uint32_t baud = buf[head+1]
                              | (buf[head+2] << 8)
                              | (buf[head+3] << 16)
                              | (buf[head+4] << 24);
                /* TODO: apply baud somewhere */
                Log_print("netsio: requested baud rate %u\n", baud);
                (void)baud;
                head += 5;
                continue;
            }

            /* SYNC RESPONSE: 1 + 5 bytes */
            if (cmd == NETSIO_SYNC_RESPONSE) {
                if (remaining < 6) break;
                /* TODO: figure out how to pause the emulator and restart here after sync */
                Log_print("netsio: sync response rcvd");
                head += 5;
                continue;
            }

            /* DATA_BYTE or DATA_BYTE_SYNC: length = 2 */
            if ((cmd == NETSIO_DATA_BYTE) ||
                (cmd == NETSIO_DATA_BYTE_SYNC)) {
                if (remaining < 2) break;
                memcpy(packet, buf + head, 2);
                enqueue_to_emulator(packet, 2);
                head += 2;
                continue;
            }

            /* DATA_BLOCK: length = 3 + payload */
            if (cmd == NETSIO_DATA_BLOCK) {
                if (remaining < 3) break;
                uint16_t L = buf[head+1] | (buf[head+2] << 8);
                if (remaining < 3 + L) break;
                memcpy(packet, buf + head, 3 + L);
                enqueue_to_emulator(packet, 3 + L);
                head += 3 + L;
                continue;
            }

            /* unknown cmd: drop it */
            head++;
        }

        /* slide leftover bytes to front */
        if (head > 0) {
            memmove(buf, buf + head, tail - head);
            tail -= head;
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