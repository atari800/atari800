//
//  Atari800Dispatch.m
//  Atari800EmulationCore
//
//  Created by Rod Münch on 01/01/2018.
//  Copyright © 2018 Atari800 development team. All rights reserved.
//

#import "Atari800Dispatch.h"

void AlwaysAsync(dispatch_block_t block)
{
    dispatch_async(dispatch_get_main_queue(), block);
}

void AsyncIfRequired(dispatch_block_t block)
{
    if ([NSThread isMainThread]) {
        
        block();
        return;
    }
    
    AlwaysAsync(block);
}
