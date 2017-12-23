//
//  Atari800ControlManager.m
//  Atari800EmulationCore-macOS
//
//  Created by Rod Münch on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800ControlManager.h"

void ControlManagerMessagePrint(char *message)
{
    NSString *messageString = [NSString stringWithUTF8String:message];
    
    NSLog(@"%@", messageString);
}

@implementation Atari800ControlManager

@end
