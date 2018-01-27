//
//  Atari800Emulator.m
//  Atari800EmulationCore
//
//  Created by Rod Münch on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800Emulator.h"
#import "Atari800MetalRenderer.h"
#import "Atari800AudioDriver.h"
#import "Atari800EmulationThread.h"
#if TARGET_OS_SIMULATOR
#import "Atari800QuartzRenderer.h"
#endif
#import "Atari800Emulator+CartridgeHandling.h"

@interface Atari800Emulator() {
    
    Atari800EmulationThread *_emulationThread;
}

@end

@implementation Atari800Emulator

static Atari800Emulator *shared = nil;

- (instancetype)init
{
    self = [super init];
    
    if (self) {
        
#if TARGET_OS_SIMULATOR
        _renderer = [[Atari800QuartzRenderer alloc] init];
#else
        _renderer = [[Atari800MetalRenderer alloc] init];
#endif
        _audioDriver = [[Atari800AudioDriver alloc] init];
    }
    
    return self;
}

- (void)reset
{
    if (_emulationThread) {
        
        Atari800UICommandEnqueue(Atari800CommandReset, Atari800CommandParamNotRequired, 0, @[], nil);
    }
}

- (void)startEmulation
{
    @synchronized (self) {
        
        if (!_emulationThread) {
            
            _emulationThread = [[Atari800EmulationThread alloc] initWithEmulator:self];
            [_emulationThread setThreadPriority:0.75];
            [_emulationThread start];
        }
    }
}

- (void)loadFile:(NSURL *)url completion:(Atari800CompletionHandler)completion
{
    NSParameterAssert(url);
    NSString *path = [url path];
    NSString *extension = [[path pathExtension] lowercaseString];
    
    if ([extension isEqualToString:@"rom"]) {
        
        [self insertCartridge:path
                   completion:completion];
        return;
    }
    
    if ([extension isEqualToString:@"xex"]) {
        
        Atari800UICommandEnqueue(Atari800CommandBinaryLoad, Atari800CommandParamNotRequired, 0, @[path], completion);
    
        return;
    }
    else if ([extension isEqualToString:@"atr"] || [extension isEqualToString:@"atx"]) {
        
        // TODO: Support multiple drives
        Atari800UICommandEnqueue(Atari800CommandMountDisk, Atari800CommandParamNotRequired, 0, @[path], completion);
    
        return;
    }
    else if ([extension isEqualToString:@"cas"]) {
        
        Atari800UICommandEnqueue(Atari800CommandLoadCassette, Atari800CommandParamNotRequired, 0, @[path], completion);
    
        return;
    }
    
    completion(YES, nil);
}

- (void)mount:(NSURL *)url driveNumber:(NSInteger)driveNumber completion:(Atari800CompletionHandler)completion
{
    NSString *path = [url path];
    Atari800UICommandEnqueue(Atari800CommandMountDisk, Atari800CommandParamNotRequired, driveNumber, @[path], completion);
}

- (void)dismount:(NSInteger)driveNumber completion:(Atari800CompletionHandler)completion
{
    Atari800UICommandEnqueue(Atari800CommandDismountDisk, Atari800CommandParamNotRequired, driveNumber, @[], completion);
}

- (void)removeCartridge:(Atari800CompletionHandler)completion
{
    Atari800UICommandEnqueue(Atari800CommandRemoveCartridge, Atari800CommandParamNotRequired, 0, @[], completion);
}

- (void)stopEmulation
{
    [_emulationThread cancel];
    _emulationThread = nil;
}

- (void)setIsNTSC:(BOOL)isNTSC completion:(Atari800CompletionHandler)completion
{
    if (_isNTSC != isNTSC) {

        Atari800UICommandParamType param = isNTSC ? Atari800CommandParamNTSCVideoSystem : Atari800CommandParamPALVideoSystem;
        
        Atari800UICommandEnqueue(Atari800CommandChangeVideoSystem, param, 0, @[], completion);

        _isNTSC = isNTSC;
    }
}

+ (instancetype)shared
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        
        shared = [[Atari800Emulator alloc] init];
    });
    
    return shared;
}

@end
