/*
 *  OBRingBuffer.c
 *  Obelisk
 *
 *  Created by Simon Lawrence on 21/12/2010.
 *  Copyright 2010 __MyCompanyName__.
 *
 *  See: BeatForce
 *    ringbuffer.c  - ring buffer
 *
 *    Copyright (c) 2001, Patrick Prasse (patrick.prasse@gmx.net)
 *
 */

#include "OBRingBuffer.h"

#include <stdlib.h>
#include <string.h>

unsigned int ring_buffer_init(struct OBRingBuffer **rb, unsigned int size) 
{
    struct OBRingBuffer *ring;
	
    if (rb == NULL || size <= 1024) {
        return 0;
    }
    
    ring = malloc(sizeof(struct OBRingBuffer));
    if (ring == NULL) {
        // Out of memory
        return 0;
    }
    memset(ring, OB_SILENCE, sizeof(struct OBRingBuffer));
	
    ring->size = 1;

    while(ring->size <= size)
        ring->size <<= 1;
	
	
    ring->rd_pointer = 0;
    ring->wr_pointer = 0;
    ring->buffer = malloc(sizeof(char)*(ring->size));
    memset(ring->buffer, 0x00, ring->size);
    *rb = ring;
	
    return 1;
}

void ring_buffer_uninit(struct OBRingBuffer *rb) 
{
	if (rb != NULL) {
		free(rb->buffer);
		free(rb);
	}
}

unsigned int ring_buffer_write(struct OBRingBuffer *rb, const unsigned char * buf, unsigned int len)
{
    unsigned int total;
    unsigned int i;
	
    total = ring_buffer_free(rb);
    if (len > total)
        len = total;
    else
        total = len;
	
    i = rb->wr_pointer;
    if (i + len > rb->size) {
        memcpy(rb->buffer + i, buf, rb->size - i);
        buf += rb->size - i;
        len -= rb->size - i;
        i = 0;
    }
    memcpy(rb->buffer + i, buf, len);
    rb->wr_pointer = i + len;

    return total;
}

unsigned int ring_buffer_write_silence(struct OBRingBuffer *rb, unsigned int len) 
{
    unsigned char * silence = malloc(len);
	memset(silence, OB_SILENCE, len);
	
    unsigned int total = ring_buffer_write(rb, silence, len);
	
	free(silence);
	
	return total;
}

unsigned int ring_buffer_free(struct OBRingBuffer *rb) 
{
    return (rb->size - 1 - ring_buffer_data_size(rb));
}


unsigned int ring_buffer_read(struct OBRingBuffer *rb, unsigned char * buf, unsigned int max) 
{
    unsigned int total;
    unsigned int i;
    
	total = ring_buffer_data_size(rb);
	
    if(max > total)
        max = total;
    else
        total = max;
	
    i = rb->rd_pointer;
    if (i + max > rb->size) {
        memcpy(buf, rb->buffer + i, rb->size - i);
        buf += rb->size - i;
        max -= rb->size - i;
        i = 0;
    }
    memcpy(buf, rb->buffer + i, max);
    rb->rd_pointer = i + max;
	
    return total;
}

unsigned int ring_buffer_data_size(struct OBRingBuffer *rb) 
{
    return ((rb->wr_pointer - rb->rd_pointer) & (rb->size-1));
}


unsigned int ring_buffer_clear(struct OBRingBuffer *rb) 
{
    memset(rb->buffer, 0, rb->size);
    rb->rd_pointer=0;
    rb->wr_pointer=0;
	
    return 0;
}
