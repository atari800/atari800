/*
 * fujinet.h - FujiNet‑PC virtual SIO device interface
 */
#ifndef FUJINET_H
#define FUJINET_H
#include "config.h"
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <unistd.h>
#endif
/* Initialize FujiNet‑PC proxy to host:port. Returns 0 on success */
int fujinet_init(const char *host, int port);
/* Send raw SIO command to FujiNet‑PC bridge; receive response bytes.
   Returns number of response bytes, or 0 on failure/no response. */
int fujinet_process_command(const unsigned char *cmd, int len,
                           unsigned char *response, int response_maxlen);
/* Teardown FujiNet‑PC connection */
void fujinet_teardown(void);

/* Stubs when FujiNet support disabled */
/*static inline int fujinet_init(const char *host, int port) { (void)host; (void)port; return 0; }
static inline int fujinet_process_command(const unsigned char *cmd, int len,
                                          unsigned char *response, int response_maxlen)
{ (void)cmd; (void)len; (void)response; (void)response_maxlen; return 0; }
static inline void fujinet_teardown(void) {}
*/
#endif /* FUJINET_H */