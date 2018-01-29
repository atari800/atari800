//
//  Atari800EmulationThread.h
//  Atari800EmulationCore
//
//  Created by Rod Münch on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Atari800Emulator.h"

typedef NS_ENUM(NSInteger, Atari800UICommandType) {
    
    Atari800CommandBinaryLoad        = 0,
    Atari800CommandMountDisk         = 1,
    Atari800CommandInsertCartridge   = 2,
    Atari800CommandLoadCassette      = 3,
    Atari800CommandReset             = 4,
    Atari800CommandRemoveCartridge   = 5,
    Atari800CommandDismountDisk      = 6,
    Atari800CommandChangeVideoSystem = 7,
    Atari800CommandChangeRAMSize     = 8,
};

typedef NS_ENUM(NSInteger, Atari800UICommandParamType) {
 
    Atari800CommandParamNotRequired = -1,
    Atari800CommandParamLeftCartridge,
    Atari800CommandParamRightCartridge,
    Atari800CommandParamPALVideoSystem,
    Atari800CommandParamNTSCVideoSystem
};

@class Atari800EmulationThread;

void Atari800UICommandEnqueue(Atari800UICommandType command, Atari800UICommandParamType param, NSInteger intParam, NSArray<NSString *> *parameters, Atari800CompletionHandler completion);

@interface Atari800EmulationThread : NSThread

- (instancetype)initWithEmulator:(Atari800Emulator *)emulator;

@end
