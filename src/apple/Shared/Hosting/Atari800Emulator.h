//
//  Atari800Emulator.h
//  Atari800EmulationCore-iOS
//
//  Created by Rod Münch on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol Atari800Renderer;
@class Atari800AudioDriver;

typedef void (^Atari800KeyboardHandler)(int *keycode, int *shiftKey, int *ctrlKey);

@interface Atari800Emulator : NSObject

@property (nonatomic, readonly, strong) id<Atari800Renderer> renderer;
@property (nonatomic, strong) Atari800AudioDriver *audioDriver;
@property (nonatomic, copy) Atari800KeyboardHandler keyboardHandler;

- (void)startEmulation;
- (void)stopEmulation;
- (void)pauseEmulation;

+ (instancetype)shared;

@end
