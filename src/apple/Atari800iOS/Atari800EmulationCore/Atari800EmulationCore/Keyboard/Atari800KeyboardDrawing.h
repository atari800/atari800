//
//  Atari800KeyboardDrawing.h
//  Atari800Keyboard
//
//  Created by Rod Münch on 02/02/2012.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Atari800KeyLayer.h"

typedef enum A8SpecialKeyCode {
    Atari800KeyCodeShift = -1,
    Atari800KeyCodeCtrl = -2,
} A8SpecialKey;

@interface Atari800KeyboardDrawing : NSObject<CALayerDelegate> {
@public
    CGColorSpaceRef colorSpace;
    CGColorRef defaultKeyColor;
    CGColorRef orangeKeyColor;
    CGColorRef yellowHighlightColor;
    CGColorRef keyboardBackgroundColor;
    CGColorRef clearColor;
    CGColorRef whiteColor;
    CGColorRef blackColor;
    CGColorRef fiftyPercentBlack;
}

- (CALayer *)layerForKey:(NSString *)text withTag:(NSUInteger)tag andUpperLeftText:(NSString *)upperLeftText andUpperText:(NSString *)upperText withStyle:(Atari800KeyLayerStyle)keyStyle withWidth:(CGFloat)width andHeight:(CGFloat)height;
- (void)addKeyLayersToLayer:(CALayer *)layer;


@end
