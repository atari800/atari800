//
//  Atari800EmulationThread.m
//  Atari800EmulationCore
//
//  Created by Rod Münch on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800EmulationThread.h"
#import "Atari800Emulator.h"
#import "Atari800Renderer.h"
#import <libkern/OSAtomic.h>

#include "antic.h"
#include "atari.h"
#include "binload.h"
#include "gtia.h"
#include "input.h"
#include "akey.h"
#include "log.h"
#include "monitor.h"
#include "platform.h"
#include "cartridge.h"

typedef struct Atari800UICommand {
    void *nextCommand;
    char **parameters;
    Atari800UICommandParamType param;
    NSInteger intParam;
    NSInteger numberOfParameters;
    Atari800UICommandType command;
} Atari800UICommand;

@interface Atari800EmulationThread() {
@public
    Atari800Emulator *_emulator;
    __unsafe_unretained id<Atari800Renderer> _renderer;
    __unsafe_unretained Atari800KeyboardHandler _keyboardHandler;
    __unsafe_unretained Atari800PortHandler _portHandler;
    BOOL _running;
    BOOL _paused;
}

@end

static Atari800EmulationThread *atari800EmulationThread;
OSQueueHead Atari800UICommands = OS_ATOMIC_QUEUE_INIT;
const NSInteger Atari800CommandNoIntParam = -1;
const size_t Atari800UICommandMaximumParameterLength = 0x1000;

void Atari800UICommandEnqueue(Atari800UICommandType command, Atari800UICommandParamType param, NSInteger intParam, NSArray<NSString *> *parameters)
{
    NSInteger numberOfParameters = [parameters count];
    char **parametersArray = (numberOfParameters == 0) ? NULL : calloc(numberOfParameters, sizeof(char *));
    
    for (NSInteger i = 0; i < numberOfParameters; ++i) {
        
        const char *parameter = [parameters[i] UTF8String];
        char *parameterCopy = calloc(strnlen(parameter, Atari800UICommandMaximumParameterLength) + 1, sizeof(char));
        strcpy(parameterCopy, parameter);
        parametersArray[i] = parameterCopy;
    }
    
    Atari800UICommand *uiCommand = calloc(1, sizeof(Atari800UICommand));
    uiCommand->command = command;
    uiCommand->parameters = parametersArray;
    uiCommand->numberOfParameters = numberOfParameters;
    uiCommand->param = param;
    uiCommand->intParam = intParam;
    
    OSAtomicEnqueue(&Atari800UICommands, uiCommand, offsetof(Atari800UICommand, nextCommand));
}

Atari800UICommand *Atari800UICommandDequeue(void)
{
    return (Atari800UICommand *)OSAtomicDequeue(&Atari800UICommands, offsetof(Atari800UICommand, nextCommand));
}

void Atari800UICommandFree(Atari800UICommand *command)
{
    if (!command)
        return;
    
    if (command->parameters) {
        
        for (int i = 0; i < command->numberOfParameters; ++i) {
            
            free(command->parameters[i]);
        }
        
        free(command->parameters);
        command->parameters = NULL;
    }
    
    free(command);
}

int VIDEOMODE_80_column;

void PLATFORM_DisplayScreen(void) {}

int PLATFORM_Exit(int run_monitor) {
    
    return noErr;
}

int PLATFORM_Initialise(int *argc, char *argv[]) { return TRUE; }

int PLATFORM_Keyboard(void) { return AKEY_NONE; }

int PLATFORM_PORT(int num) {
    
    if (!atari800EmulationThread->_portHandler) {
        
        return 0xFF;
    }
    
    int port = 0xFF;
    int trig = 0x01;
    
    atari800EmulationThread->_portHandler(num, &port, &trig);
    
    return port;
}

int PLATFORM_TRIG(int num) {
    
    if (!atari800EmulationThread->_portHandler) {
        
        return 0x01;
    }
    
    int port = 0xFF;
    int trig = 0x01;
    
    atari800EmulationThread->_portHandler(num, &port, &trig);
    
    return trig;
}

int VIDEOMODE_Set80Column(int value)
{
    
    VIDEOMODE_80_column = value;
    return value;
}

NS_INLINE void Atari800BinaryLoad(__unsafe_unretained Atari800EmulationThread *thread, Atari800UICommand *command)
{
    NSCAssert(command->numberOfParameters == 1, @"No parameters specified for binary load");
    const char *exeName = command->parameters[0];
    BINLOAD_Loader(exeName);
}

NS_INLINE void Atari800InsertCartridge(__unsafe_unretained Atari800EmulationThread *thread, Atari800UICommand *command)
{
    NSCAssert(command->numberOfParameters == 1, @"No parameters specified for cartridge");
    const char *cartridgeName = command->parameters[0];
    
    int r = CARTRIDGE_Insert(cartridgeName);
    switch (r) {
        case CARTRIDGE_CANT_OPEN:
            NSLog(@"Can't open %s", CARTRIDGE_main.filename);
            break;
        case CARTRIDGE_BAD_FORMAT:
            NSLog(@"Unknown cartridge format");
            break;
        case CARTRIDGE_BAD_CHECKSUM:
            NSLog(@"Warning: bad CART checksum");
            break;
        case 0:
            /* ok */
        default:
            /* r > 0 */
            CARTRIDGE_SetTypeAutoReboot(&CARTRIDGE_main, (int)command->intParam);
            break;
    }
}

NS_INLINE void Atari800RemoveCartridge(__unsafe_unretained Atari800EmulationThread *thread, Atari800UICommand *command)
{
    CARTRIDGE_RemoveAutoReboot();
}

NS_INLINE void Atari800Reset(__unsafe_unretained Atari800EmulationThread *thread, Atari800UICommand *command)
{
    Atari800_Warmstart();
}

NS_INLINE void Atari800ProcessUICommand(__unsafe_unretained Atari800EmulationThread *thread, Atari800UICommand *command)
{
    switch (command->command) {
        
        case Atari800CommandBinaryLoad:
            Atari800BinaryLoad(thread, command);
            break;
            
        case Atari800CommandInsertCartridge:
            Atari800InsertCartridge(thread, command);
            break;
            
        case Atari800CommandInsertDisk:
            
            break;
            
        case Atari800CommandLoadCassette:
            
            break;
        
        case Atari800CommandReset:
            Atari800Reset(thread, command);
            break;
            
        case Atari800CommandRemoveCartridge:
            Atari800RemoveCartridge(thread, command);
            break;
            
        default:
            break;
    }
}

NS_INLINE void Atari800ProcessUICommands(__unsafe_unretained Atari800EmulationThread *thread)
{
    while (true) {
        
        Atari800UICommand *command = Atari800UICommandDequeue();
        
        if (command == NULL) {
            
            return;
        }
        
        Atari800ProcessUICommand(thread, command);
        
        Atari800UICommandFree(command);
    }
}

NS_INLINE void Atari800ProcessKeyboardInput(__unsafe_unretained Atari800EmulationThread *thread)
{
    if (atari800EmulationThread->_keyboardHandler) {
        
        int keycode = 0;
        int controlKey = 0;
        int shiftKey = 0;
        int console = 0;
        
        atari800EmulationThread->_keyboardHandler(&keycode, &shiftKey, &controlKey, &console);
        
        INPUT_key_code = (shiftKey) ? keycode | AKEY_SHFT : keycode;
        
        INPUT_key_consol = console;
    }
    else {
        
        INPUT_key_code = AKEY_NONE;
    }
}

void Atari800StartEmulation(__unsafe_unretained Atari800EmulationThread *thread)
{
    atari800EmulationThread = thread;
    
    NSBundle *bundle = [NSBundle bundleForClass:NSClassFromString(@"Atari800EmulationThread")];
    const char *bundlePath = [[bundle.resourcePath stringByAppendingPathComponent:@"Resources"] UTF8String];
    int argc = 1;
    const char **argv = {&bundlePath};
    
    Screen_atari = (unsigned int *)atari800EmulationThread->_renderer.screen;
    
    if (!Atari800_Initialise(&argc, (char **)argv)) {
        
        return;
    }
    
    atari800EmulationThread->_running = YES;
    
    while (atari800EmulationThread->_running) {
        
        Atari800ProcessUICommands(atari800EmulationThread);
        
        Atari800ProcessKeyboardInput(atari800EmulationThread);
        
        Atari800_Frame();
    }
}

@implementation Atari800EmulationThread

- (instancetype)initWithEmulator:(Atari800Emulator *)emulator
{
    self = [super init];
    
    if (self) {
        
        _emulator = emulator;
        _keyboardHandler = emulator.keyboardHandler;
        _portHandler = emulator.portHandler;
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
    _paused = YES;
}

- (void)cancel
{
    _running = NO;
    [super cancel];
}

@end
