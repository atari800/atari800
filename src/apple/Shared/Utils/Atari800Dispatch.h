//
//  Atari800Dispatch.h
//  Atari800EmulationCore-iOS
//
//  Created by Simon Lawrence on 01/01/2018.
//  Copyright © 2018 Atari800 development team. All rights reserved.
//

#import <Foundation/Foundation.h>

void AlwaysAsync(dispatch_block_t block);
void AsyncIfRequired(dispatch_block_t block);
