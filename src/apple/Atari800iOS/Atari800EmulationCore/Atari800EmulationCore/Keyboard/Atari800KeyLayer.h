//
//  Atari800KeyLayer.h
//  Atari800Keyboard
//
//  Created by Rod Münch on 02/02/2012.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import <QuartzCore/QuartzCore.h>

typedef enum Atari800KeyLayerStyle {
    Atari800KeySimple,
    Atari800KeyUpperLower,
    Atari800KeyThreePart,
    Atari800KeyThreePartSymbol,
    Atari800KeyAtari,
    Atari800KeyCtrl,
    Atari800KeyShift,
} Atari800KeyLayerStyle;

@interface Atari800KeyLayer : CALayer {
@public
    NSInteger tag;
}

@property (nonatomic, assign) NSInteger tag;
@property (nonatomic, copy) NSString *text;
@property (nonatomic, copy) NSString *upperText;
@property (nonatomic, copy) NSString *upperLeftText;
@property (nonatomic, assign) Atari800KeyLayerStyle keyStyle;

@end
