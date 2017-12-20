//
//  Atari800Platform.m
//  Atari800EmulationCore-macOS
//
//  Created by Simon Lawrence on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800Platform.h"

int VIDEOMODE_80_column;

void PLATFORM_DisplayScreen(void) {}

int PLATFORM_Exit(int run_monitor) { return noErr; }

int PLATFORM_Initialise(int *argc, char *argv[]) { return noErr; }

int PLATFORM_Keyboard(void) { return 0; }

int PLATFORM_PORT(int num) { return 0xff; }

int PLATFORM_TRIG(int num) { return 1; }

int VIDEOMODE_Set80Column(int value) {
    VIDEOMODE_80_column = value;
    return value;
}

@implementation Atari800Platform

@end
