/* NetSIO to FujiNet-PC */
#include "netsio.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <stdio.h>

/* Outbound FIFO (to FujiNet-PC) */
static uint8_t out_fifo[NETSIO_FIFO_SIZE];
static int out_head = 0;
static int out_tail = 0;
static pthread_mutex_t out_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  out_cond = PTHREAD_COND_INITIALIZER;

/* Inbound FIFO (from FujiNet-PC) */
static uint8_t in_fifo[NETSIO_FIFO_SIZE];
static int in_head = 0;
static int in_tail = 0;
static pthread_mutex_t in_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  in_cond = PTHREAD_COND_INITIALIZER;

static int sockfd = -1;
static struct sockaddr_in peer_addr;
static pthread_t netsio_thread;
static volatile int netsio_running = 0;

/* Forward declare the thread function */
static void *netsio_thread_func(void *arg);

int netsio_init(const char *host, uint16_t port)
{
    struct hostent *he;
    const char *h;
    uint16_t p;
    int rc;

    h = (host && host[0] != '\0') ? host : "127.0.0.1";
    p = (port != 0) ? port : 9996;

    he = gethostbyname(h);
    if (he == NULL) {
        return -1;
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    peer_addr.sin_port   = htons(p);
    /* set the host address */
    /* memcpy(&peer_addr.sin_addr,
           he->h_addr_list[0], he->h_length); */

    netsio_running = 1;
    rc = pthread_create(&netsio_thread, NULL,
                        netsio_thread_func, NULL);
    if (rc != 0) {
        netsio_running = 0;
        close(sockfd);
        return -1;
    }

    if (bind(sockfd, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) < 0) {
        perror("netsio bind");
        close(sockfd);
        return -1;
    }

    printf("[netsio] running on %s:%u\n", h, p);
    return 0;
}

void netsio_shutdown(void)
{
    if (!netsio_running) {
        return;
    }
    pthread_mutex_lock(&out_lock);
    netsio_running = 0;
    pthread_cond_signal(&out_cond);
    pthread_mutex_unlock(&out_lock);
    pthread_join(netsio_thread, NULL);
    close(sockfd);
    sockfd = -1;
}

int netsio_send_byte(uint8_t b)
{
    int next;

    pthread_mutex_lock(&out_lock);
    next = (out_tail + 1) % NETSIO_FIFO_SIZE;
    if (next == out_head) {
        /* FIFO full */
        pthread_mutex_unlock(&out_lock);
        return -1;
    }

    out_fifo[out_tail] = b;
    out_tail = next;
    pthread_cond_signal(&out_cond);
    pthread_mutex_unlock(&out_lock);
    return 0;
}

int netsio_recv_byte(uint8_t *b)
{
    if (b == NULL) {
        return -1;
    }

    pthread_mutex_lock(&in_lock);
    if (in_head == in_tail) {
        /* FIFO empty */
        pthread_mutex_unlock(&in_lock);
        return -1;
    }

    *b = in_fifo[in_head];
    in_head = (in_head + 1) % NETSIO_FIFO_SIZE;
    pthread_mutex_unlock(&in_lock);
    return 0;
}

/* process any special NetSIO commands that need an immediate reply */
static void netsio_handle_command(uint8_t cmd) {
    switch (cmd) {
        case 0xC2: {  /* Ping request */
            uint8_t resp = 0xC3;  /* Ping response */
            pthread_mutex_lock(&out_lock);
            int next = (out_tail + 1) % NETSIO_FIFO_SIZE;
            if (next != out_head) {
                out_fifo[out_tail] = resp;
                out_tail = next;
                pthread_cond_signal(&out_cond);
            }
            pthread_mutex_unlock(&out_lock);
            printf("netsio: PING received\n");
            break;
        }
        case 0xC4: {  /* Alive request */
            uint8_t resp = 0xC5; /* Alive response */
            pthread_mutex_lock(&out_lock);
            int next = (out_tail + 1) % NETSIO_FIFO_SIZE;
            if (next != out_head) {
                out_fifo[out_tail] = resp;
                out_tail = next;
                pthread_cond_signal(&out_cond);
            }
            pthread_mutex_unlock(&out_lock);
            printf("netsio: ALIVE received\n");
            break;
        }
        default:
            break;
    }
}

static void *netsio_thread_func(void *arg)
{
    fd_set rfds, wfds;
    int has_out;
    int nfds;
    int result;
    uint8_t byte;
    struct timeval tv;

    (void)arg;

    while (netsio_running) {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_SET(sockfd, &rfds);

        pthread_mutex_lock(&out_lock);
        has_out = (out_head != out_tail);
        pthread_mutex_unlock(&out_lock);
        if (has_out) {
            FD_SET(sockfd, &wfds);
        }

        nfds = sockfd + 1;
        /* Use a timeout to check netsio_running periodically */
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        result = select(nfds, &rfds, &wfds, NULL, &tv);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (result == 0) {
            /* timeout */
            continue;
        }

        if (FD_ISSET(sockfd, &rfds)) {
            uint8_t buf[512];
            int i = 0;
            struct sockaddr_in from;
            socklen_t fromlen = sizeof(from);
            int rlen = recvfrom(sockfd, buf, sizeof(buf), 0,
                        (struct sockaddr*)&from, &fromlen);
            if (rlen > 0) {
                peer_addr = from;
                for (i = 0; i < rlen; i++) {
                    uint8_t b = buf[i];
        
                    /* DEBUG print the incoming byte */
                    printf("[netsio RX] 0x%02X\n", b);
        
                    pthread_mutex_lock(&in_lock);
                    int next = (in_tail + 1) % NETSIO_FIFO_SIZE;
                    if (next != in_head) {
                        in_fifo[in_tail] = b;
                        in_tail = next;
                        pthread_cond_signal(&in_cond);
                    }
                    pthread_mutex_unlock(&in_lock);
        
                    /* Handle immediate commands */
                    netsio_handle_command(b);
                }
            }
        }

        if (FD_ISSET(sockfd, &wfds)) {
            pthread_mutex_lock(&out_lock);
            if (out_head != out_tail) {
                byte = out_fifo[out_head];
                out_head = (out_head + 1) % NETSIO_FIFO_SIZE;
                pthread_mutex_unlock(&out_lock);
                sendto(sockfd, &byte, 1, 0,
                       (struct sockaddr *)&peer_addr,
                       sizeof(peer_addr));
            } else {
                pthread_mutex_unlock(&out_lock);
            }
        }
    }

    return NULL;
}