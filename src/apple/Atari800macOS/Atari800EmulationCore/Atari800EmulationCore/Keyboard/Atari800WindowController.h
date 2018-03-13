//
//  Atari800WindowController.h
//  Atari800EmulationCore
//
//  Created by Rod Münch on 25/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import <Atari800EmulationCore/Atari800Emulator.h>
@import AppKit;

@interface Atari800WindowController : NSWindowController

- (Atari800KeyboardHandler)newKeyboardHandler;
- (Atari800PortHandler)newPortHandler;

@end
