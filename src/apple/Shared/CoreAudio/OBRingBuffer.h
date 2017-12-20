/*
 *  OBRingBuffer.h
 *  Obelisk
 *
 *  Created by Simon Lawrence on 21/12/2010.
 *  Copyright 2010 __MyCompanyName__.
 *
 *  See: BeatForce
 *    ringbuffer.h  - ring buffer (header)
 *
 *    Copyright (c) 2001, Patrick Prasse (patrick.prasse@gmx.net)
 *
 */
#ifndef __OB_RINGBUFFER_H__
#define __OB_RINGBUFFER_H__

#define OB_RINGBUFFER_MAGIC  0x72627546    /* 'rbuF' */
// DONE: this doesnt need to vary based on codec as it's on the linear PCM side
#define OB_SILENCE 0x00
#define OB_INITIAL_SILENCE_LENGTH (160 * 2) * 10 // 10 Frames of 16 bit PCM
#define rb_magic_check(var,err)  {if(var->magic!=RINGBUFFER_MAGIC) return err;}

typedef struct OBRingBuffer {
    char *buffer;
    unsigned int wr_pointer;
    unsigned int rd_pointer;
    long magic;
    unsigned int size;
} OBRingBuffer;


/* ring buffer functions */
unsigned int ring_buffer_init(struct OBRingBuffer **, unsigned int);

void ring_buffer_uninit(struct OBRingBuffer *rb);

unsigned int ring_buffer_write(struct OBRingBuffer *, const unsigned char *, unsigned int);

unsigned int ring_buffer_write_silence(struct OBRingBuffer *rb, unsigned int len);

unsigned int ring_buffer_free(struct OBRingBuffer *);

unsigned int ring_buffer_read(struct OBRingBuffer *, unsigned char *, unsigned int);

unsigned int ring_buffer_data_size(struct OBRingBuffer *);

unsigned int ring_buffer_clear(struct OBRingBuffer *);

#endif

