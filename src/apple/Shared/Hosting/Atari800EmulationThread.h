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
    
    Atari800CommandBinaryLoad      = 0,
    Atari800CommandInsertDisk      = 1,
    Atari800CommandInsertCartridge = 2,
    Atari800CommandLoadCassette    = 3,
    Atari800CommandReset           = 4,
    Atari800CommandRemoveCartridge = 5,
};

typedef NS_ENUM(NSInteger, Atari800UICommandParamType) {
 
    Atari800CommandParamNotRequired = -1,
    Atari800CommandParamDrive0 = 0,
    Atari800CommandParamLeftCartridge,
    Atari800CommandParamRightCartridge,
};

@class Atari800EmulationThread;

void Atari800UICommandEnqueue(Atari800UICommandType command, Atari800UICommandParamType param, NSInteger intParam, NSArray<NSString *> *parameters);

@interface Atari800EmulationThread : NSThread

- (instancetype)initWithEmulator:(Atari800Emulator *)emulator;

- (void)pause;

@end
