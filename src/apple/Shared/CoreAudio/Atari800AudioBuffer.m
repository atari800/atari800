//
//  Atari800AudioBuffer.m
//  Atari800EmulationCore-macOS
//
//  Created by Simon Lawrence on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800AudioBuffer.h"

@interface Atari800AudioBuffer() {
@public
    size_t _size;
    uint8_t *_buffer;
    size_t _writePointer;
    size_t _readPointer;
}

@end

@implementation Atari800AudioBuffer

- (instancetype)initWithCapacity:(size_t)capacity
{
    self = [super init];
    
    if (self) {
    
        _buffer = calloc(capacity, sizeof(uint8_t));
        _readPointer = 0;
        _writePointer = 0;
        
        _size = 1;
        
        while (_size <= capacity)
            _size <<= 1;
    }
    
    return self;
}

@end

size_t Atari800AudioBufferWrite(__unsafe_unretained Atari800AudioBuffer *destination, const uint8_t *source, size_t length)
{
    size_t total;
    size_t i;
    
    total = Atari800AudioBufferFreeSpace(destination);
    if (length > total)
        length = total;
    else
        total = length;
    
    i = destination->_writePointer;
    if (i + length > destination->_size) {
        memcpy(destination->_buffer + i, source, destination->_size - i);
        source += destination->_size - i;
        length -= destination->_size - i;
        i = 0;
    }
    memcpy(destination->_buffer + i, source, length);
    destination->_writePointer = i + length;
    
    return total;
}

size_t Atari800AudioBufferFreeSpace(__unsafe_unretained Atari800AudioBuffer *buffer)
{
    return (buffer->_size - 1 - Atari800AudioBufferDataSize(buffer));
}

size_t Atari800AudioBufferRead(__unsafe_unretained Atari800AudioBuffer *source, unsigned char *destination, size_t max)
{
    size_t total;
    size_t i;
    
    total = Atari800AudioBufferDataSize(source);
    
    if (max > total)
        max = total;
    else
        total = max;
    
    i = source->_readPointer;
    
    if ((i + max) > source->_size) {
        memcpy(destination, source->_buffer + i, source->_size - i);
        destination += source->_size - i;
        max -= source->_size - i;
        i = 0;
    }
    
    memcpy(destination, source->_buffer + i, max);
    source->_readPointer = i + max;
    
    return total;
}

size_t Atari800AudioBufferDataSize(__unsafe_unretained Atari800AudioBuffer *buffer)
{
    return ((buffer->_writePointer - buffer->_readPointer) & (buffer->_size - 1));
}

void Atari800AudioBufferClear(__unsafe_unretained Atari800AudioBuffer *buffer)
{
    memset(buffer->_buffer, 0, buffer->_size);
}

