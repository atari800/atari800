//
//  Atari800Emulator.m
//  Atari800EmulationCore-iOS
//
//  Created by Simon Lawrence on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800Emulator.h"
#import "Atari800Renderer.h"
#import "Atari800AudioDriver.h"

@implementation Atari800Emulator

static Atari800Emulator *shared = nil;

+ (instancetype)shared
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        
        shared = [[Atari800Emulator alloc] init];
    });
    
    return shared;
}

@end
