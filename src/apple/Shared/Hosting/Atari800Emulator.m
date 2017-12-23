//
//  Atari800Emulator.m
//  Atari800EmulationCore-iOS
//
//  Created by Rod Münch on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800Emulator.h"
#import "Atari800MetalRenderer.h"
#import "Atari800AudioDriver.h"
#import "Atari800EmulationThread.h"
#if TARGET_OS_SIMULATOR
#import "Atari800QuartzRenderer.h"
#endif

@interface Atari800Emulator() {
    
    Atari800EmulationThread *_emulationThread;
}

@end

@implementation Atari800Emulator

static Atari800Emulator *shared = nil;

- (instancetype)init
{
    self = [super init];

    if (self) {
        
#if TARGET_OS_SIMULATOR
        _renderer = [[Atari800QuartzRenderer alloc] init];
#else
        _renderer = [[Atari800MetalRenderer alloc] init];
#endif
    }
    
    return self;
}

- (void)startEmulation
{
    _emulationThread = [[Atari800EmulationThread alloc] initWithEmulator:self];
    [_emulationThread setThreadPriority:0.75];
    [_emulationThread start];
}

- (void)pauseEmulation
{
    [_emulationThread pause];
}

- (void)stopEmulation
{
    [_emulationThread cancel];
}

+ (instancetype)shared
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        
        shared = [[Atari800Emulator alloc] init];
    });
    
    return shared;
}

@end
