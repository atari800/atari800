/* NetSIO to FujiNet-PC */
#ifndef NETSIO_H
#define NETSIO_H

#include <stdint.h>
#include <pthread.h>

/* NetSIO Commands */
#define NETSIO_DATA_BYTE          0x01
#define NETSIO_DATA_BLOCK         0x02
#define NETSIO_DATA_BYTE_SYNC     0x09
#define NETSIO_COMMAND_OFF        0x10
#define NETSIO_COMMAND_ON         0x11
#define NETSIO_COMMAND_OFF_SYNC   0x18
#define NETSIO_MOTOR_OFF          0x20
#define NETSIO_MOTOR_ON           0x21
#define NETSIO_PROCEED_OFF        0x30
#define NETSIO_PROCEED_ON         0x31
#define NETSIO_INTERRUPT_OFF      0x40
#define NETSIO_INTERRUPT_ON       0x41
#define NETSIO_SPEED_CHANGE       0x80
#define NETSIO_SYNC_RESPONSE      0x81
/* NetSIO Connection Management */
#define NETSIO_DEVICE_DISCONNECTED 0xC0
#define NETSIO_DEVICE_CONNECTED   0xC1
#define NETSIO_PING_REQUEST       0xC2
#define NETSIO_PING_RESPONSE      0xC3
#define NETSIO_ALIVE_REQUEST      0xC4
#define NETSIO_ALIVE_RESPONSE     0xC5
#define NETSIO_CREDIT_STATUS      0xC6
#define NETSIO_CREDIT_UPDATE      0xC7
/* NetSIO Notifications */
#define NETSIO_WARM_RESET         0xFE
#define NETSIO_COLD_RESET         0xFF

/* FIFO buffer depth */
#define NETSIO_FIFO_SIZE 4096

/* NetSIO message struct */
typedef struct NetSIOMsg {
    uint8_t id;
    uint8_t arg[512];
    size_t arg_len;
    double tstamp;
} NetSIOMsg;

extern int netsio_enabled;

/* Initialize NetSIO subsystem, connecting to FujiNet-PC at host:port. */
/* Returns 0 on success, non-zero on error. */
int netsio_init(uint16_t port);

/* Shutdown NetSIO, join the thread, close socket. */
void netsio_shutdown(void);

/* Enqueue one byte to send to FujiNet-PC. */
/* Returns 0 on success, -1 if FIFO is full. */
int netsio_send_byte(uint8_t b);

/* Dequeue one byte received from FujiNet-PC. */
/* Returns 0 on success, -1 if FIFO is empty. */
int netsio_recv_byte(uint8_t *b);

#endif /* NETSIO_H */