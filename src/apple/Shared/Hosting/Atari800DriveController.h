//
//  Atari800DriveController.h
//  Atari800EmulationCore
//
//  Created by Rod Münch on 07/01/2018.
//  Copyright © 2018 Atari800 development team. All rights reserved.
//

#import <Foundation/Foundation.h>

extern NSString *const Atari800DrivesUpdatedNotification;
extern NSString *const Atari800DriveNumberKey;
extern NSString *const Atari800DriveFileNameKey;

@interface Atari800DriveController : NSObject

@property (nonatomic, readonly, strong) NSArray<NSDictionary *> *drives;

- (BOOL)mount:(NSURL *)url driveNumber:(NSInteger)driveNumber error:(NSError **)error;
- (void)dismount:(NSInteger)driveNumber;

@end
