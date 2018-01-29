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
#import "Atari800Dispatch.h"

#import <libkern/OSAtomic.h>

#import "antic.h"
#import "atari.h"
#import "binload.h"
#import "gtia.h"
#import "input.h"
#import "akey.h"
#import "log.h"
#import "monitor.h"
#import "platform.h"
#import "cartridge.h"
#import "sio.h"
#import "memory.h"

#if __has_feature(objc_arc)
    #error This file must be compiled without ARC. Use -fno-objc-arc flag
#endif

typedef struct Atari800UICommand {
    void *nextCommand;
    char **parameters;
    Atari800UICommandParamType param;
    NSInteger intParam;
    NSInteger numberOfParameters;
    Atari800UICommandType command;
    Atari800CompletionHandler handler;
} Atari800UICommand;

@interface Atari800EmulationThread() {
@public
    __unsafe_unretained Atari800Emulator *_emulator;
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

void Atari800UICommandEnqueue(Atari800UICommandType command, Atari800UICommandParamType param, NSInteger intParam, NSArray<NSString *> *parameters, Atari800CompletionHandler completion)
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
    uiCommand->handler = Block_copy(completion);
    
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
        
        if (command->handler) {
            Block_release(command->handler);
            command->handler = nil;
        }
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

NS_INLINE void Atari800CompleteCommand(__unsafe_unretained Atari800EmulationThread *thread, Atari800UICommand *command, BOOL ok, NSError *error) {
    
    if (!command->handler) {
        
        return;
    }
    
    Atari800CompletionHandler handlerCopy = Block_copy(command->handler);
    
    AlwaysAsync(^{
        
        handlerCopy(ok, error);
        Block_release(handlerCopy);
    });
}

NS_INLINE void Atari800MountDisk(__unsafe_unretained Atari800EmulationThread *thread, Atari800UICommand *command)
{
    NSCAssert(command->numberOfParameters == 1, @"No parameters specified for disk mount");
    const char *imageName = command->parameters[0];
    SIO_Mount((int)command->intParam + 1, imageName, FALSE);
    Atari800CompleteCommand(thread, command, YES, nil);
}

NS_INLINE void Atari800DismountDisk(__unsafe_unretained Atari800EmulationThread *thread, Atari800UICommand *command)
{
    SIO_Dismount((int)command->intParam + 1);
    Atari800CompleteCommand(thread, command, YES, nil);
}

NS_INLINE void Atari800BinaryLoad(__unsafe_unretained Atari800EmulationThread *thread, Atari800UICommand *command)
{
    NSCAssert(command->numberOfParameters == 1, @"No parameters specified for binary load");
    const char *exeName = command->parameters[0];
    BINLOAD_Loader(exeName);
    Atari800CompleteCommand(thread, command, YES, nil);
}

NS_INLINE void Atari800InsertCartridge(__unsafe_unretained Atari800EmulationThread *thread, Atari800UICommand *command)
{
    NSCAssert(command->numberOfParameters == 1, @"No parameters specified for cartridge");
    const char *cartridgeName = command->parameters[0];
    
    int r = CARTRIDGE_Insert(cartridgeName);
    switch (r) {
        case CARTRIDGE_CANT_OPEN:
            NSLog(@"Can't open %s", CARTRIDGE_main.filename);
            Atari800CompleteCommand(thread, command, NO, nil);
            break;
        case CARTRIDGE_BAD_FORMAT:
            NSLog(@"Unknown cartridge format");
            Atari800CompleteCommand(thread, command, NO, nil);
            break;
        case CARTRIDGE_BAD_CHECKSUM:
            NSLog(@"Warning: bad CART checksum");
            Atari800CompleteCommand(thread, command, NO, nil);
            break;
        case 0:
            /* ok */
        default:
            /* r > 0 */
            CARTRIDGE_SetTypeAutoReboot(&CARTRIDGE_main, (int)command->intParam);
            Atari800CompleteCommand(thread, command, YES, nil);
            break;
    }
}

NS_INLINE void Atari800ChangeVideoSystem(__unsafe_unretained Atari800EmulationThread *thread, Atari800UICommand *command)
{
    int tvMode = Atari800_tv_mode;
    
    switch (command->param) {
        case Atari800CommandParamNTSCVideoSystem:
            tvMode = Atari800_TV_NTSC;
            break;
          
        case Atari800CommandParamPALVideoSystem:
            tvMode = Atari800_TV_PAL;
            break;
            
        default:
            break;
    }
    
    if (tvMode != Atari800_tv_mode) {
    
        Atari800_SetTVMode(tvMode);
        Atari800_InitialiseMachine();
    }
    Atari800CompleteCommand(thread, command, YES, nil);
}

NS_INLINE void Atari800ChangeRAMSize(__unsafe_unretained Atari800EmulationThread *thread, Atari800UICommand *command)
{
    int ramSize = (int)command->intParam;

    if (MEMORY_ram_size != ramSize) {
        
        MEMORY_ram_size = ramSize;
        Atari800_InitialiseMachine();
    }
    
    Atari800CompleteCommand(thread, command, YES, nil);
}

NS_INLINE void Atari800RemoveCartridge(__unsafe_unretained Atari800EmulationThread *thread, Atari800UICommand *command)
{
    CARTRIDGE_RemoveAutoReboot();
    Atari800CompleteCommand(thread, command, YES, nil);
}

NS_INLINE void Atari800Reset(__unsafe_unretained Atari800EmulationThread *thread, Atari800UICommand *command)
{
    Atari800_Coldstart();
    Atari800CompleteCommand(thread, command, YES, nil);
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
            
        case Atari800CommandMountDisk:
            Atari800MountDisk(thread, command);
            break;
            
        case Atari800CommandLoadCassette:
            
            break;
        
        case Atari800CommandReset:
            Atari800Reset(thread, command);
            break;
            
        case Atari800CommandRemoveCartridge:
            Atari800RemoveCartridge(thread, command);
            break;
            
        case Atari800CommandDismountDisk:
            Atari800DismountDisk(thread, command);
            break;
          
        case Atari800CommandChangeVideoSystem:
            Atari800ChangeVideoSystem(thread, command);
            break;
           
        case Atari800CommandChangeRAMSize:
            Atari800ChangeRAMSize(thread, command);
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
    
    Screen_atari = (unsigned int *)thread->_renderer.screen;
    
    if (!Atari800_Initialise(&argc, (char **)argv)) {
        
        return;
    }
    
    thread->_running = YES;
    
    while (thread->_running) {
        
        Atari800ProcessUICommands(thread);
        
        Atari800ProcessKeyboardInput(thread);
        
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

- (void)dealloc
{
    [super dealloc];
}

- (void)main
{
    @autoreleasepool {
        
        Atari800StartEmulation(self);
    }
}

- (void)cancel
{
    _running = NO;
    [super cancel];
}

@end
