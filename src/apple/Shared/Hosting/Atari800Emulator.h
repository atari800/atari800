//
//  Atari800Emulator.h
//  Atari800EmulationCore-iOS
//
//  Created by Rod Münch on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol Atari800Renderer;
@class Atari800AudioDriver;
@class Atari800Emulator;

typedef void (^Atari800KeyboardHandler)(int *keycode, int *shiftKey, int *ctrlKey, int *console);
typedef void (^Atari800PortHandler)(int port, int *stick, int *trig);
typedef void (^Atari800CompletionHandler)(BOOL ok, NSError *error);
typedef void (^Atari800CartridgeSelectionHandler)(BOOL ok, NSInteger cartridgeType);

@protocol Atari800EmulatorDelegate<NSObject>

- (void)emulator:(Atari800Emulator *)emulator didSelectCartridgeWithSize:(NSInteger)size filename:(NSString *)filename completion:(Atari800CartridgeSelectionHandler)completion;

@end

@interface Atari800Emulator : NSObject

@property (nonatomic, readonly, strong) id<Atari800Renderer> renderer;
@property (nonatomic, readonly, strong) Atari800AudioDriver *audioDriver;

@property (nonatomic, copy) Atari800KeyboardHandler keyboardHandler;
@property (nonatomic, copy) Atari800PortHandler portHandler;

- (NSDictionary<NSNumber *, NSString *> *)supportedCartridgeTypesForSize:(NSInteger)sizeKb;

@property (nonatomic, weak) id<Atari800EmulatorDelegate> delegate;

- (void)startEmulation;
- (void)stopEmulation;
- (void)pauseEmulation;

- (void)removeCartridge;
- (void)reset;

- (void)loadFile:(NSURL *)url completion:(Atari800CompletionHandler)completion;

+ (instancetype)shared;

@end
