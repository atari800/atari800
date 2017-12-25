//
//  Atari800WindowController.h
//  Atari800EmulationCore-macOS
//
//  Created by Simon Lawrence on 25/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import <Atari800EmulationCore_macOS/Atari800Emulator.h>
@import AppKit;

@interface Atari800WindowController : NSWindowController

- (Atari800KeyboardHandler)newKeyboardHandler;

@end
