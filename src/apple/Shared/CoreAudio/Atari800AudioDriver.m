//
//  Atari800AudioDriver.m
//  Atari800EmulationCore-macOS
//
//  Created by Simon Lawrence on 19/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800AudioDriver.h"
#import "sound.h"

@implementation Atari800AudioDriver

@end

int PLATFORM_SoundSetup(Sound_setup_t *setup) {
    return 0;
}
void PLATFORM_SoundExit(void) {}
void PLATFORM_SoundPause(void) {}
void PLATFORM_SoundContinue(void) {}
unsigned int PLATFORM_SoundAvailable(void) {
    return 0;
}
void PLATFORM_SoundWrite(UBYTE const *buffer, unsigned int size) {}
