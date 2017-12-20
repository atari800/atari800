//
//  Atari800AudioBuffer.h
//  Atari800EmulationCore-macOS
//
//  Created by Simon Lawrence on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface Atari800AudioBuffer : NSObject

- (instancetype)initWithCapacity:(size_t)capacity;

@end

size_t Atari800AudioBufferWrite(__unsafe_unretained Atari800AudioBuffer *, const uint8_t *, size_t);
size_t Atari800AudioBufferFreeSpace(__unsafe_unretained Atari800AudioBuffer *);
size_t Atari800AudioBufferRead(__unsafe_unretained Atari800AudioBuffer *, unsigned char *, size_t);
size_t Atari800AudioBufferDataSize(__unsafe_unretained Atari800AudioBuffer *);
void Atari800AudioBufferClear(__unsafe_unretained Atari800AudioBuffer *);
