//
//  Atari800Renderer.h
//  Atari800EmulationCore-macOS
//
//  Created by Rod Münch on 22/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol Atari800Renderer <NSObject>

@property (nonatomic, readonly) uint8_t *screen;

- (void)setupForView:(id)view widthInPixels:(NSInteger)width heightInScanLines:(NSInteger)height;

@end
