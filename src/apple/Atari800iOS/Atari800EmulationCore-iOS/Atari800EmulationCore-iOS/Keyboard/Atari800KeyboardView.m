//
//  Atari800KeyboardView.m
//  Atari800Keyboard
//
//  Created by Rod Münch on 02/02/2012.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800KeyboardView.h"
@import QuartzCore;
@import CoreText;
#import "Atari800Emulator.h"
#import "Atari800KeyLayer.h"
#import "Atari800KeyboardDrawing.h"
#import "akey.h"

@interface Atari800KeyboardView() {
@private
    Atari800KeyboardDrawing *_drawing;
    CALayer *_lastKey;
    CABasicAnimation *_keyDownAnimation;
    CABasicAnimation *_keyUpAnimation;
@public
    int keycode;
    int shiftKey;
    int ctrlKey;
    int console;
}

@end

@implementation Atari800KeyboardView

const CGFloat KEY_DOWN_SCALE = 1.1f;
const NSTimeInterval KEY_ANIMATION_DURATION = 0.05f;

const uint8_t AtariStickUp    = 0x01;
const uint8_t AtariStickDown  = 0x02;
const uint8_t AtariStickLeft  = 0x04;
const uint8_t AtariStickRight = 0x08;

const uint8_t AtariConsoleStart  = 0x01;
const uint8_t AtariConsoleSelect = 0x02;
const uint8_t AtariConsoleOption = 0x04;

const uint8_t AtariStickUpMask    = 0xFE;
const uint8_t AtariStickDownMask  = 0xFD;
const uint8_t AtariStickLeftMask  = 0xFB;
const uint8_t AtariStickRightMask = 0xF7;

const uint8_t AtariConsoleStartMask  = 0xFE;
const uint8_t AtariConsoleSelectMask = 0xFD;
const uint8_t AtariConsoleOptionMask = 0xFB;

#pragma mark - View lifecycle

- (void)internalInit
{
    [self registerFonts];
    
    keycode = AKEY_NONE;
    console = 0x07;
    
    _drawing = [[Atari800KeyboardDrawing alloc] init];
    
    CABasicAnimation *keyUpAnimation = [CABasicAnimation animation];
    keyUpAnimation.fillMode = kCAFillModeForwards;
    keyUpAnimation.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
    keyUpAnimation.duration = KEY_ANIMATION_DURATION;
    keyUpAnimation.repeatCount = 0.0f;
    keyUpAnimation.autoreverses = NO;
    keyUpAnimation.fromValue = [NSNumber numberWithFloat:1.0f / KEY_DOWN_SCALE];
    keyUpAnimation.toValue = [NSNumber numberWithFloat:1.0f];
    keyUpAnimation.keyPath = @"contentsScale";
    keyUpAnimation.removedOnCompletion = YES;
    _keyUpAnimation = keyUpAnimation;
    
    CABasicAnimation *keyDownAnimation = [CABasicAnimation animation];
    keyDownAnimation.fillMode = kCAFillModeForwards;
    keyDownAnimation.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseIn];
    keyDownAnimation.duration = KEY_ANIMATION_DURATION;
    keyDownAnimation.repeatCount = 0.0f;
    keyDownAnimation.autoreverses = NO;
    keyDownAnimation.fromValue = [NSNumber numberWithFloat:1.0f];
    keyDownAnimation.toValue = [NSNumber numberWithFloat:1.0f / KEY_DOWN_SCALE];
    keyDownAnimation.keyPath = @"contentsScale";
    keyDownAnimation.removedOnCompletion = NO;
    _keyDownAnimation = keyDownAnimation;
}

- (void)registerFonts
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        
        NSBundle *bundle = [NSBundle bundleForClass:NSClassFromString(@"Atari800KeyboardView")];
        
        NSString *fontPath = [bundle pathForResource:@"SF Atarian System" ofType:@"ttf" inDirectory:@"fonts/SFAtarianSystem"];
        
        NSData *fontData = [NSData dataWithContentsOfFile:fontPath];
        CFErrorRef error;
        CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)fontData);
        CGFontRef font = CGFontCreateWithDataProvider(provider);
        if (! CTFontManagerRegisterGraphicsFont(font, &error)) {
            CFStringRef errorDescription = CFErrorCopyDescription(error);
            NSLog(@"Failed to load font: %@", errorDescription);
            CFRelease(errorDescription);
        }
        CFRelease(font);
        CFRelease(provider);
        
#ifdef DEBUG
        for (NSString *familyName in [[UIFont familyNames] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]) {
            
            NSLog(@"%@", familyName);
            for (NSString *fontName in [[UIFont fontNamesForFamilyName:familyName] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]) {
                NSLog(@"\t%@", fontName);
            }
        }
#endif
    });
}

- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        // Initialization code
        [self internalInit];
    }
    return self;
}

- (id)initWithCoder:(NSCoder *)aDecoder
{
    self = [super initWithCoder:aDecoder];
    if (self) {
        // Initialization code
        [self internalInit];
    }
    
    return self;
}

- (void)awakeFromNib
{
    [super awakeFromNib];
    
    [_drawing addKeyLayersToLayer:self.layer];
    self.backgroundColor = [UIColor colorWithCGColor:_drawing->keyboardBackgroundColor];
    
    for (UIButton *button in @[self.startButton, self.selectButton, self.optionButton]) {
        [button addTarget:self
                   action:@selector(consoleKeyDown:)
         forControlEvents:UIControlEventTouchDown];
        [button addTarget:self
                   action:@selector(consoleKeyUp:)
         forControlEvents:UIControlEventTouchUpInside];
        [button addTarget:self
                   action:@selector(consoleKeyUp:)
         forControlEvents:UIControlEventTouchUpOutside];
    }
}

#pragma mark - Touch event handling

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        CGPoint thePoint = [touch locationInView:self];
        thePoint = [self.layer convertPoint:thePoint 
                                    toLayer:self.layer.superlayer];
        CALayer *theLayer = [self.layer hitTest:thePoint];
        if ([theLayer isKindOfClass:[Atari800KeyLayer class]]) {
            Atari800KeyLayer *taggedLayer = (Atari800KeyLayer *)theLayer;
            
            switch (taggedLayer->tag) {
                case Atari800KeyCodeCtrl:
                    ctrlKey = YES;
                    break;
                case Atari800KeyCodeShift:
                    shiftKey = YES;
                    break;
                default:
                    keycode = (int)taggedLayer->tag;
                    break;
            }
            
            [CATransaction begin];
            [CATransaction setValue:(id)kCFBooleanTrue
                             forKey:kCATransactionDisableActions];
            [theLayer removeFromSuperlayer];
            [self.layer addSublayer:theLayer];
            [CATransaction commit];
            
            [theLayer addAnimation:_keyDownAnimation
                            forKey:@"keypress"];
        }
     }
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        CGPoint thePoint = [touch locationInView:self];
        thePoint = [self.layer convertPoint:thePoint 
                                    toLayer:self.layer.superlayer];
        CALayer *theLayer = [self.layer hitTest:thePoint];
        if ([theLayer isKindOfClass:[Atari800KeyLayer class]]) {
            Atari800KeyLayer *taggedLayer = (Atari800KeyLayer *)theLayer;
            
            switch (taggedLayer->tag) {
                case Atari800KeyCodeCtrl:
                    ctrlKey = NO;
                    break;
                case Atari800KeyCodeShift:
                    shiftKey = NO; 
                    break;
                default:
                    keycode = AKEY_NONE;
                    break;
            }

            [theLayer addAnimation:_keyUpAnimation
                            forKey:@"keypress"];
        }
    }
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        // TODO: Cancel invalid keypresses, trigger new ones
    }    
}

- (Atari800KeyboardHandler)newKeyboardHandler
{
    __unsafe_unretained Atari800KeyboardView *uSelf = self;
    
    Atari800KeyboardHandler handler = ^(int *keycode, int *shiftKey, int *ctrlKey, int *console) {
        
        *keycode = uSelf->keycode;
        *shiftKey = uSelf->shiftKey;
        *ctrlKey = uSelf->ctrlKey;
        *console = uSelf->console;
    };
    
    return handler;
}

- (IBAction)consoleKeyDown:(id)sender
{
    if (sender == self.startButton) {
        console &= AtariConsoleStartMask;
    }
    else if (sender == self.selectButton) {
        console &= AtariConsoleSelectMask;
    }
    else if (sender == self.optionButton) {
        console &= AtariConsoleOptionMask;
    }
}

- (IBAction)consoleKeyUp:(id)sender
{
    if (sender == self.startButton) {
        console |= AtariConsoleStart;
    }
    else if (sender == self.selectButton) {
        console |= AtariConsoleSelect;
    }
    else if (sender == self.optionButton) {
        console |= AtariConsoleOption;
    }
}

@end
