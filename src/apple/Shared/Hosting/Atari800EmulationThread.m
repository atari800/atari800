//
//  Atari800EmulationThread.m
//  Atari800EmulationCore-macOS
//
//  Created by Simon Lawrence on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800EmulationThread.h"
#import "Atari800Emulator.h"
#import "Atari800Renderer.h"

#include "antic.h"
#include "atari.h"
#include "binload.h"
#include "gtia.h"
#include "input.h"
#include "akey.h"
#include "log.h"
#include "monitor.h"
#include "platform.h"

@interface Atari800EmulationThread() {
@public
    Atari800Emulator *_emulator;
    __unsafe_unretained Atari800Renderer *_renderer;
    BOOL _atari800Running;
}

@end

static Atari800EmulationThread *atari800EmulationThread;

int VIDEOMODE_80_column;


void PLATFORM_DisplayScreen(void)
{
    memcpy(atari800EmulationThread->_renderer.screen, Screen_atari, 384 * 240 * sizeof(uint8_t));
}

int PLATFORM_Exit(int run_monitor) {
    
    return noErr;
}

int PLATFORM_Initialise(int *argc, char *argv[])
{
    
    return TRUE;
}

int PLATFORM_Keyboard(void) { return 0; }

int PLATFORM_PORT(int num) { return 0xff; }

int PLATFORM_TRIG(int num) { return 1; }

int VIDEOMODE_Set80Column(int value)
{
    
    VIDEOMODE_80_column = value;
    return value;
}

void Atari800StartEmulation(__unsafe_unretained Atari800EmulationThread *thread)
{
    atari800EmulationThread = thread;
    
    NSBundle *bundle = [NSBundle bundleForClass:NSClassFromString(@"Atari800EmulationThread")];
    const char * bundlePath = [[bundle.resourcePath stringByAppendingPathComponent:@"Resources"] UTF8String];
    int argc = 1;
    char **argv = {&bundlePath};
    
    if (!Atari800_Initialise(&argc, argv)) {
        
        return;
    }
    
    atari800EmulationThread->_atari800Running = YES;
    
    // HACK: Load something for now
    NSString *exeFilePath = [bundle pathForResource:@"yoomp_pa"
                                             ofType:@"xex"
                                        inDirectory:@"XEX"];
    const char *exeName = [exeFilePath cStringUsingEncoding:NSUTF8StringEncoding];
    BINLOAD_Loader(exeName);
    
    while (atari800EmulationThread->_atari800Running) {
        
        INPUT_key_code = PLATFORM_Keyboard();
        Atari800_Frame();
        if (Atari800_display_screen)
            PLATFORM_DisplayScreen();
    }
}

@implementation Atari800EmulationThread

- (instancetype)initWithEmulator:(Atari800Emulator *)emulator
{
    self = [super init];
    
    if (self) {
        
        _emulator = emulator;
        _renderer = emulator.renderer;
    }
    
    return self;
}

- (void)main
{
    @autoreleasepool {
        Atari800StartEmulation(self);
    }
}

- (void)pause
{
    // TODO: Implement this
}

- (void)cancel
{
    _atari800Running = NO;
    [super cancel];
}

@end
