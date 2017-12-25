//
//  Atari800EmulationThread.h
//  Atari800EmulationCore-macOS
//
//  Created by Rod Münch on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Atari800Emulator.h"

@interface Atari800EmulationThread : NSThread

- (instancetype)initWithEmulator:(Atari800Emulator *)emulator;

- (void)pause;

@end
