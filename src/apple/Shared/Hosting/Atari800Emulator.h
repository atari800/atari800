//
//  Atari800Emulator.h
//  Atari800EmulationCore-iOS
//
//  Created by Simon Lawrence on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import <Foundation/Foundation.h>

@class Atari800Renderer;
@class Atari800AudioDriver;

@interface Atari800Emulator : NSObject

@property (nonatomic, strong) Atari800Renderer *renderer;
@property (nonatomic, strong) Atari800AudioDriver *audioDriver;

- (void)startEmulation;
- (void)stopEmulation;
- (void)pauseEmulation;

+ (instancetype)shared;

@end
