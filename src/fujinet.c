/*
 * fujinet.c - FujiNet‑PC virtual SIO device implementation
 */
#include "config.h"
#include "fujinet.h"
#include <stdio.h>
#include <string.h>

static int fujinet_sock = -1;
static struct sockaddr_in fujinet_addr;

int fujinet_init(const char *host, int port)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        return -1;
    }
#endif
    if (fujinet_sock != -1) return 0;
    fujinet_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (fujinet_sock < 0) {
        perror("fujinet socket");
        return -1;
    }
    memset(&fujinet_addr, 0, sizeof(fujinet_addr));
    fujinet_addr.sin_family = AF_INET;
    fujinet_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &fujinet_addr.sin_addr) <= 0) {
        struct hostent *h = gethostbyname(host);
        if (!h) return -1;
        memcpy(&fujinet_addr.sin_addr, h->h_addr_list[0], h->h_length);
    }
    /* Cold reset FujiNet‑PC bridge via netsio protocol */
    {
        uint8_t reset_cmd = 0xFF;
        /* ignore errors */
        sendto(fujinet_sock, &reset_cmd, sizeof(reset_cmd), 0, 
               (struct sockaddr *)&fujinet_addr, sizeof(fujinet_addr));
    }
    
    return 0;
}

int fujinet_process_command(const unsigned char *cmd, int len,
                           unsigned char *response, int response_maxlen)
{
    if (fujinet_sock < 0) return 0;
    /* send command */
    if (sendto(fujinet_sock, cmd, len, 0,
               (struct sockaddr *)&fujinet_addr,
               sizeof(fujinet_addr)) < 0) {
        return 0;
    }
    /* set timeout for response */
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(fujinet_sock, SOL_SOCKET, SO_RCVTIMEO,
               (char *)&tv, sizeof(tv));
    /* receive response */
    int n = recvfrom(fujinet_sock, response, response_maxlen, 0, NULL, NULL);
    if (n <= 0) return 0;
    return n;
}

void fujinet_teardown(void)
{
    if (fujinet_sock >= 0) {
#ifdef _WIN32
        closesocket(fujinet_sock);
        WSACleanup();
#else
        close(fujinet_sock);
#endif
        fujinet_sock = -1;
    }
}
