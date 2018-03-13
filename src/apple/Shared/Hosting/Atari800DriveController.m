//
//  Atari800DriveController.m
//  Atari800EmulationCore
//
//  Created by Rod Münch on 07/01/2018.
//  Copyright © 2018 Atari800 development team. All rights reserved.
//

#import "Atari800DriveController.h"
#import "Atari800Emulator.h"
#import "sio.h"

NSString *const Atari800DrivesUpdatedNotification = @"Atari800DrivesUpdatedNotification";
NSString *const Atari800DriveNumberKey = @"Atari800DriveNumberKey";
NSString *const Atari800DriveFileNameKey = @"Atari800DriveFileNameKey";

@implementation Atari800DriveController

- (NSArray<NSDictionary *> *)drives
{
    NSMutableArray *result = [NSMutableArray array];
    
    for (NSInteger driveNumber = 0; driveNumber < 8; ++driveNumber) {
        
        NSString *imageName = [[NSString stringWithUTF8String:SIO_filename[driveNumber]] lastPathComponent];
        
        [result addObject:@{Atari800DriveNumberKey: @(driveNumber + 1),
                            Atari800DriveFileNameKey: imageName}];
    }
    
    return result;
}

- (BOOL)mount:(NSURL *)url driveNumber:(NSInteger)driveNumber error:(NSError **)error
{
    if (driveNumber < 0 || driveNumber > 7)
        return NO;
    
    [[Atari800Emulator shared] mount:url
                         driveNumber:driveNumber
                          completion:^(BOOL ok, NSError *error) {
                              [[NSNotificationCenter defaultCenter] postNotificationName:Atari800DrivesUpdatedNotification
                                                                                  object:self];
                          }];
    return YES;
}

- (void)dismount:(NSInteger)drive
{
    [[Atari800Emulator shared] dismount:keyDriveNumber completion:^(BOOL ok, NSError *error) {
        [[NSNotificationCenter defaultCenter] postNotificationName:Atari800DrivesUpdatedNotification
                                                            object:self];
    }];
}

@end
