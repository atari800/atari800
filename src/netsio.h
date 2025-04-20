/* NetSIO to FujiNet-PC */
#ifndef NETSIO_H
#define NETSIO_H

#include <stdint.h>
#include <pthread.h>

/* FIFO buffer depth */
#define NETSIO_FIFO_SIZE 4096

int netsio_enabled = 0;

/* Initialize NetSIO subsystem, connecting to FujiNet-PC at host:port. */
/* Returns 0 on success, non-zero on error. */
int netsio_init(const char *host, uint16_t port);

/* Shutdown NetSIO, join the thread, close socket. */
void netsio_shutdown(void);

/* Enqueue one byte to send to FujiNet-PC. */
/* Returns 0 on success, -1 if FIFO is full. */
int netsio_send_byte(uint8_t b);

/* Dequeue one byte received from FujiNet-PC. */
/* Returns 0 on success, -1 if FIFO is empty. */
int netsio_recv_byte(uint8_t *b);

#endif /* NETSIO_H */