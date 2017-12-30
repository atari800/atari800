//
//  Atari800Emulator+CartridgeHandling.h
//  Atari800EmulationCore-macOS
//
//  Created by Rod Münch on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800Emulator.h"

@interface Atari800Emulator (CartridgeHandling)

- (void)insertCartridge:(NSString *)path completion:(Atari800CompletionHandler)completion;

@end
