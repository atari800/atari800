//
//  Atari800KeyboardDrawing.m
//  Atari800Keyboard
//
//  Created by Rod Münch on 02/02/2012.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800KeyboardDrawing.h"
#import "Atari800KeyLayer.h"
#import "Atari800KeyboardView.h"
#import "akey.h"
@import QuartzCore;
@import CoreText;

@implementation Atari800KeyboardDrawing

const CGFloat defaultKeyWidth = 40.0f;
const CGFloat defaultKeyHeight = 40.0f;
const CGFloat keyGutter = 10.0f;

char * const Atari800KeyboardFontFamily = "SFAtarianSystem";
char * const Atari800KeyboardSymbolFontFamily = "HiraginoSans-W3";

const CGFloat SMALL_FONT_SIZE = 11.0f;
const CGFloat MED_FONT_SIZE = 13.0f;
const CGFloat LARGE_FONT_SIZE = 22.0f;
const CGFloat borderShadowOffset = 2.0f;

#define CGPointIntegral(x, y) CGPointMake(floorf(x), floorf(y))
#define CenterOffset(w) ((w - defaultKeyWidth) / 2.0f)

- (id)init
{
    self = [super init];

    if (self) {
        colorSpace = CGColorSpaceCreateDeviceRGB();
        
        defaultKeyColor = [[UIColor colorWithRed:122.0f/255.0f 
                                           green:73.0/255.0f 
                                            blue:37.0/255.0f 
                                           alpha:1.0f] CGColor];
        CGColorRetain(defaultKeyColor);
        
        orangeKeyColor = [[UIColor colorWithRed:202.0f/255.0f 
                                          green:102.0f/255.0f 
                                           blue:34.0f/255.0f 
                                          alpha:1.0f] CGColor];
        CGColorRetain(orangeKeyColor);
        
        yellowHighlightColor = [[UIColor colorWithRed:255.0f/255.0f 
                                                green:167.0f/255.0f 
                                                 blue:69.0f/255.0f 
                                                alpha:1.0f] CGColor];
        CGColorRetain(yellowHighlightColor);
        
        keyboardBackgroundColor = [[UIColor colorWithRed:33.0f/255.0f 
                                                   green:23.0f/255.0f 
                                                    blue:14.0f/255.0f 
                                                   alpha:1.0f] CGColor];
        CGColorRetain(keyboardBackgroundColor);
        
        clearColor = [[UIColor clearColor] CGColor];
        CGColorRetain(clearColor);
        
        whiteColor = [[UIColor whiteColor] CGColor];
        CGColorRetain(whiteColor);
        
        blackColor = [[UIColor blackColor] CGColor];
        CGColorRetain(blackColor);
        
        fiftyPercentBlack = [[UIColor colorWithWhite:0.0f 
                                               alpha:0.5f] CGColor];
        CGColorRetain(fiftyPercentBlack);
    }
    
    return self;
}

- (void)dealloc
{
    CGColorRelease(defaultKeyColor);
    CGColorRelease(orangeKeyColor);
    CGColorRelease(yellowHighlightColor);
    CGColorRelease(keyboardBackgroundColor);
    CGColorRelease(clearColor);
    CGColorRelease(whiteColor);
    CGColorRelease(blackColor);
    CGColorRelease(fiftyPercentBlack);
    
    CGColorSpaceRelease(colorSpace);
}

- (void)drawKeyBorderInContext:(CGContextRef)context withRadius:(CGFloat)radius atOrigin:(CGPoint)origin withWidth:(CGFloat)width andHeight:(CGFloat)height andColor:(CGColorRef)color withShadow:(BOOL)shadow
{
    CGRect roundedRect = CGRectIntegral(CGRectInset(CGRectMake(origin.x, origin.y, width, height), 1.0f, 1.0f));
    
    CGContextSaveGState(context);
    CGSize shadowSize = CGSizeZero;
    if (shadow)
        shadowSize = CGSizeMake(0.0f, 2.0f);
    CGContextSetShadowWithColor(context, shadowSize, 0.7f, fiftyPercentBlack);
    CGContextSetRGBStrokeColor(context, 1.0, 1.0, 1.0, 1.0); 
    CGContextSetFillColorWithColor(context, color);
    CGContextSetLineWidth(context, 2.0);
    CGFloat minx = CGRectGetMinX(roundedRect), midx = CGRectGetMidX(roundedRect), maxx = CGRectGetMaxX(roundedRect); 
    CGFloat miny = CGRectGetMinY(roundedRect), midy = CGRectGetMidY(roundedRect), maxy = CGRectGetMaxY(roundedRect); 
    CGContextMoveToPoint(context, minx, midy); 
    CGContextAddArcToPoint(context, minx, miny, midx, miny, radius); 
    CGContextAddArcToPoint(context, maxx, miny, maxx, midy, radius); 
    CGContextAddArcToPoint(context, maxx, maxy, midx, maxy, radius); 
    CGContextAddArcToPoint(context, minx, maxy, minx, midy, radius); 
    CGContextClosePath(context); 
    CGContextDrawPath(context, kCGPathFillStroke);     
    CGContextRestoreGState(context);
}

- (void)drawKeyBorderInContext:(CGContextRef)context withRadius:(CGFloat)radius atOrigin:(CGPoint)origin withWidth:(CGFloat)width andHeight:(CGFloat)height andColor:(CGColorRef)color
{
    [self drawKeyBorderInContext:context 
                      withRadius:radius 
                        atOrigin:origin 
                       withWidth:width 
                       andHeight:height 
                        andColor:color
                      withShadow:YES];
}

- (void)drawKeyInContext:(CGContextRef)context withFont:(NSString *)fontFamily withSize:(CGFloat)fontSize atOrigin:(CGPoint)origin withWidth:(CGFloat)width andHeight:(CGFloat)height withBackgroundColor:(CGColorRef)backgroundColor andText:(NSString *)text
{
    const char *textString = [text cStringUsingEncoding:NSMacOSRomanStringEncoding];
    CGContextSaveGState(context);
    
    CGContextTranslateCTM(context, 0.0f, height);
    CGContextScaleCTM(context, 1.0f, -1.0f);
    [self drawKeyBorderInContext:context 
                      withRadius:6.0f 
                        atOrigin:origin 
                       withWidth:width 
                       andHeight:height 
                        andColor:backgroundColor];
    
    CGContextSetStrokeColorWithColor(context, whiteColor);
    CGContextSetFillColorWithColor(context, whiteColor);
    
    if (fontSize == 0)
        fontSize = [text length] > 1 ? SMALL_FONT_SIZE : LARGE_FONT_SIZE;
    CGFloat textOffset = roundf((22.0 - fontSize) / 2.0f);
    if (![fontFamily isEqualToString:[NSString stringWithUTF8String:Atari800KeyboardFontFamily]]) {
        textOffset += 4.0f;
    }
    else {
        if (fontSize == LARGE_FONT_SIZE)
            textOffset += 2.0f;
    }
    CGContextSelectFont(context, [fontFamily UTF8String], fontSize, kCGEncodingMacRoman);
    CGContextSetTextPosition(context, origin.x, origin.y + 10.0f);
    CGPoint startPos = CGContextGetTextPosition(context);
    CGContextSetTextDrawingMode(context, kCGTextInvisible);
    CGContextShowTextAtPoint(context, startPos.x, startPos.y, textString, [text length]);
    CGPoint endPos = CGContextGetTextPosition(context);
    CGSize textSize = CGSizeMake(endPos.x - startPos.x, endPos.y - startPos.y);
    CGPoint textOrigin = CGPointIntegral(origin.x + roundf((width - textSize.width) / 2.0f), origin.y + 10.0f + textOffset); ///origin.y + roundf((height - textSize.height) / 2.0f));
    
    CGContextSetShadowWithColor(context, CGSizeMake(1.0f, 1.0f), 1.0f, blackColor);
    
    CGContextSetTextDrawingMode(context, kCGTextFill);
    CGContextShowTextAtPoint(context, textOrigin.x, textOrigin.y, textString, [text length]);
    CGContextRestoreGState(context);
}

- (void)drawThreePartKeyInContext:(CGContextRef)context atOrigin:(CGPoint)origin withWidth:(CGFloat)width andHeight:(CGFloat)height andUpperLeftText:(NSString *)upperLeftText inSymbolFont:(BOOL)symbolFont andUpperRightText:(NSString *)upperRightText andLowerText:(NSString *)lowerText
{
    const char *lowerTextString = [lowerText cStringUsingEncoding:NSMacOSRomanStringEncoding];
    const char *upperLeftTextString = [upperLeftText cStringUsingEncoding:(symbolFont) ? NSUnicodeStringEncoding : NSMacOSRomanStringEncoding];
    const char *upperRightTextString = [upperRightText cStringUsingEncoding:NSMacOSRomanStringEncoding];
    
    CGContextSaveGState(context);
    
    CGContextTranslateCTM(context, 0.0f, height);
    CGContextScaleCTM(context, 1.0f, -1.0f);
    
    CGFloat offset = roundf(height / 2.0f);
    CGFloat overlap = 1.0f;
    
    [self drawKeyBorderInContext:context 
                      withRadius:6.0f
                        atOrigin:CGPointIntegral(origin.x, origin.y + offset - overlap - 1) 
                       withWidth:width 
                       andHeight:offset + overlap
                        andColor:orangeKeyColor];
    
    [self drawKeyBorderInContext:context 
                      withRadius:4.0f
                        atOrigin:origin 
                       withWidth:width 
                       andHeight:offset + overlap
                        andColor:defaultKeyColor];
    
    [self drawKeyBorderInContext:context 
                      withRadius:4.0f
                        atOrigin:origin 
                       withWidth:width 
                       andHeight:height 
                        andColor:clearColor
                      withShadow:NO];
    
    CGContextSetShadowWithColor(context, CGSizeMake(1.0f, 1.0f), 0.0f,blackColor);
    
    CGContextSaveGState(context);
    
    // lower text
    CGFloat fontSize = [lowerText length] > 1 ? SMALL_FONT_SIZE : MED_FONT_SIZE;
    if ([lowerText isEqualToString:@"-"] || [lowerText isEqualToString:@"="]) {
        fontSize = 18.0f;
        offset -= 4.0f;
    }
    
    UIFont *standardFont = [UIFont fontWithName:[NSString stringWithUTF8String:Atari800KeyboardFontFamily]
                                           size:fontSize];
    NSAttributedString *attributedText = [[NSAttributedString alloc] initWithString:lowerText
                                                                         attributes:@{NSFontAttributeName: standardFont,
                                                                                      NSForegroundColorAttributeName: [UIColor whiteColor]}];
    CGContextSetStrokeColorWithColor(context, whiteColor);
    CGContextSetFillColorWithColor(context, whiteColor);
    CGContextSelectFont(context, Atari800KeyboardFontFamily, fontSize, kCGEncodingMacRoman);/////kCGEncodingFontSpecific);
    CGContextSetTextPosition(context, 0.0f, origin.y + 6.0f);
    CGPoint startPos = CGContextGetTextPosition(context);
    CGContextSetTextDrawingMode(context, kCGTextInvisible);
    CGContextShowTextAtPoint(context, startPos.x, startPos.y, lowerTextString, [lowerText length]);
    CGPoint endPos = CGContextGetTextPosition(context);
    CGSize textSize = CGSizeMake(endPos.x - startPos.x, endPos.y - startPos.y);
    CGPoint textOrigin = CGPointIntegral(origin.x + roundf((width - textSize.width) / 2.0f), origin.y + 6.0f);
    CGContextSetTextDrawingMode(context, kCGTextFill);
    CGContextShowTextAtPoint(context, textOrigin.x, textOrigin.y, lowerTextString, [lowerText length]);
    
    offset = roundf(height / 2.0f);
    // Upper left text
    fontSize = [upperLeftText length] > 1 ? SMALL_FONT_SIZE : MED_FONT_SIZE;
    CGContextSetStrokeColorWithColor(context, whiteColor);
    CGContextSetFillColorWithColor(context, whiteColor);
    if (symbolFont) {
        fontSize -= 2.0f;
        CGFontRef symbolFont = CGFontCreateWithFontName((__bridge CFStringRef)[NSString stringWithUTF8String:Atari800KeyboardSymbolFontFamily]);
        CGContextSelectFont(context, Atari800KeyboardSymbolFontFamily, fontSize, kCGEncodingMacRoman);
        CTFontRef ctFont = CTFontCreateWithGraphicsFont(symbolFont, fontSize, &CGAffineTransformIdentity, NULL);
        CGGlyph glyphs[[upperLeftText length]];
        CTFontGetGlyphsForCharacters(ctFont, (const unichar*)upperLeftTextString, glyphs, [upperLeftText length]);
        CGContextSetTextPosition(context, 0.0f, origin.y + offset + 5.0f);
        CGContextSetTextDrawingMode(context, kCGTextInvisible);
        startPos = CGContextGetTextPosition(context);
        CGContextShowGlyphsAtPoint(context, startPos.x, startPos.y, glyphs, [upperLeftText length]);
        endPos = CGContextGetTextPosition(context);
        textSize = CGSizeMake(endPos.x - startPos.x, endPos.y - startPos.y);
        textOrigin = CGPointIntegral(origin.x + roundf((width - textSize.width) / 4.0f), origin.y + offset + 5.0f); ///origin.y + roundf((height - textSize.height) / 2.0f));
        CGContextSaveGState(context);
        CGContextSetShadowWithColor(context, CGSizeMake(0.0f, 0.0f), 0.0f, blackColor);
        CGContextSetStrokeColorWithColor(context, yellowHighlightColor);
        CGContextSetFillColorWithColor(context, yellowHighlightColor);
        CGRect highlightRect = CGRectOffset(CGRectInset(CGRectMake(textOrigin.x, textOrigin.y, textSize.width, 12.0f), -2.0f, 0.0f), 1.0f, -3.0f);
        CGContextFillRect(context, highlightRect);
        CGColorRef textColor = whiteColor;
        CGContextSetStrokeColorWithColor(context, textColor);
        CGContextSetFillColorWithColor(context, textColor);
        CGContextSetTextDrawingMode(context, kCGTextFill);
        CGContextSetShadowWithColor(context, CGSizeMake(1.0f, 1.0f), 0.0f, blackColor);
        CGContextSetTextDrawingMode(context, kCGTextFill);
        CGContextShowGlyphsAtPoint(context, textOrigin.x, textOrigin.y, glyphs, [upperLeftText length]);
        CGContextRestoreGState(context);
    }
    else {
        fontSize = [upperRightText length] > 1 ? SMALL_FONT_SIZE : MED_FONT_SIZE;
        CGContextSetStrokeColorWithColor(context, whiteColor);
        CGContextSetFillColorWithColor(context, whiteColor);
        CGContextSelectFont(context, Atari800KeyboardFontFamily, fontSize, kCGEncodingMacRoman);
        CGContextSetTextPosition(context, 0.0f, origin.y + offset + 5.0f);
        CGContextSetTextPosition(context, 0.0f, origin.y + offset + 5.0f);
        CGContextSetTextDrawingMode(context, kCGTextInvisible);
        startPos = CGContextGetTextPosition(context);
        CGContextShowTextAtPoint(context, startPos.x, startPos.y, upperLeftTextString, [upperLeftText length]);
        endPos = CGContextGetTextPosition(context);
        textSize = CGSizeMake(endPos.x - startPos.x, endPos.y - startPos.y);
        textOrigin = CGPointIntegral(origin.x + roundf((width - textSize.width) / 4.0f), origin.y + offset + 5.0f); 
        CGContextSaveGState(context);
        CGContextSetShadowWithColor(context, CGSizeMake(0.0f, 0.0f), 0.0f, blackColor);
        CGContextSetStrokeColorWithColor(context, yellowHighlightColor);
        CGContextSetFillColorWithColor(context, yellowHighlightColor);
        CGRect highlightRect = CGRectIntegral(CGRectOffset(CGRectInset(CGRectMake(textOrigin.x, textOrigin.y, textSize.width, 12.0f), -2.0f, 0.0f), 1.0f, -3.0f));
        CGContextFillRect(context, highlightRect);
        CGColorRef textColor = whiteColor; 
        CGContextSetStrokeColorWithColor(context, textColor);
        CGContextSetFillColorWithColor(context, textColor);
        CGContextSetTextDrawingMode(context, kCGTextFill);
        CGContextSetShadowWithColor(context, CGSizeMake(1.0f, 1.0f), 0.0f, blackColor);
        CGContextShowTextAtPoint(context, textOrigin.x, textOrigin.y, upperLeftTextString, [upperLeftText length]);
        CGContextRestoreGState(context);
    }
    // DONE: Paint yellow background
    
    // Upper right text
    fontSize = [upperRightText length] > 1 ? SMALL_FONT_SIZE : MED_FONT_SIZE;
    CGContextSetStrokeColorWithColor(context, whiteColor);
    CGContextSetFillColorWithColor(context, whiteColor);
    CGContextSelectFont(context, Atari800KeyboardFontFamily, fontSize, kCGEncodingMacRoman);
    CGContextSetTextPosition(context, 0.0f, origin.y + offset + 5.0f);
    startPos = CGContextGetTextPosition(context);
    CGContextSetTextDrawingMode(context, kCGTextInvisible);
    CGContextShowTextAtPoint(context, startPos.x, startPos.y, upperRightTextString, [upperRightText length]);
    endPos = CGContextGetTextPosition(context);
    textSize = CGSizeMake(endPos.x - startPos.x, endPos.y - startPos.y);
    textOrigin = CGPointIntegral(origin.x + roundf((width - textSize.width) / 4.0f) * 3.0f, origin.y + offset + 5.0f); 
    CGContextSetShadowWithColor(context, CGSizeMake(1.0f, 1.0f), 0.0f, blackColor);
    CGContextSetTextDrawingMode(context, kCGTextFill);
    CGContextShowTextAtPoint(context, textOrigin.x, textOrigin.y, upperRightTextString, [upperRightText length]);
    CGContextRestoreGState(context);
}

- (void)drawSplitKeyInContext:(CGContextRef)context  atOrigin:(CGPoint)origin withWidth:(CGFloat)width andHeight:(CGFloat)height andUpperText:(NSString *)upperText andLowerText:(NSString *)lowerText
{
    CGContextSaveGState(context);
    
    CGContextTranslateCTM(context, 0.0f, height);
    CGContextScaleCTM(context, 1.0f, -1.0f);
    
    const char *lowerTextString = [lowerText cStringUsingEncoding:NSMacOSRomanStringEncoding];
    const char *upperTextString = [upperText cStringUsingEncoding:NSMacOSRomanStringEncoding];
    CGFloat offset = roundf(height / 2.0f);
    CGFloat overlap = 1.0f;
    
    [self drawKeyBorderInContext:context 
                      withRadius:6.0f
                        atOrigin:CGPointIntegral(origin.x, origin.y + offset - overlap - 1) 
                       withWidth:width 
                       andHeight:offset + overlap
                        andColor:orangeKeyColor];
    
    [self drawKeyBorderInContext:context 
                      withRadius:4.0f
                        atOrigin:origin 
                       withWidth:width 
                       andHeight:offset + overlap
                        andColor:defaultKeyColor];
    
    [self drawKeyBorderInContext:context 
                      withRadius:4.0f
                        atOrigin:origin 
                       withWidth:width 
                       andHeight:height 
                        andColor:clearColor
                      withShadow:NO];
    
    CGContextSetShadowWithColor(context, CGSizeMake(1.0f, 1.0f), 0.0f, blackColor);
    CGContextSaveGState(context);
    
    // Lower text
    CGFloat fontSize = [lowerText length] > 1 ? SMALL_FONT_SIZE : MED_FONT_SIZE;
    if ([lowerText isEqualToString:@"-"] || [lowerText isEqualToString:@"="]) {
        fontSize = 18.0f;
        offset -= 4.0f;
    }
    CGContextSetStrokeColorWithColor(context, whiteColor);
    CGContextSetFillColorWithColor(context, whiteColor);
    CGContextSelectFont(context, Atari800KeyboardFontFamily, fontSize, kCGEncodingMacRoman);
    CGContextSetTextPosition(context, 0.0f, origin.y + 6.0f);
    CGPoint startPos = CGContextGetTextPosition(context);
    CGContextSetTextDrawingMode(context, kCGTextInvisible);
    CGContextShowTextAtPoint(context, startPos.x, startPos.y, lowerTextString, [lowerText length]);
    CGPoint endPos = CGContextGetTextPosition(context);
    CGSize textSize = CGSizeMake(endPos.x - startPos.x, endPos.y - startPos.y);
    CGPoint textOrigin = CGPointIntegral(origin.x + roundf((width - textSize.width) / 2.0f), origin.y + 6.0f);
    CGContextSetTextDrawingMode(context, kCGTextFill);
    CGContextSetShadowWithColor(context, CGSizeMake(1.0f, 1.0f), 0.0f, blackColor);
    CGContextShowTextAtPoint(context, textOrigin.x, textOrigin.y, lowerTextString, [lowerText length]);
    
    // Upper text
    fontSize = [upperText length] > 1 ? SMALL_FONT_SIZE : MED_FONT_SIZE;
    if ([upperText isEqualToString:@"\""] || [upperText isEqualToString:@"'"]) {
        fontSize = 18.0f;
        offset -= 4.0f;
    }
    CGContextSetStrokeColorWithColor(context, whiteColor);
    CGContextSetFillColorWithColor(context, whiteColor);
    CGContextSelectFont(context, Atari800KeyboardFontFamily, fontSize, kCGEncodingMacRoman);
    CGContextSetTextPosition(context, 0.0f, origin.y + offset + 5.0f);
    startPos = CGContextGetTextPosition(context);
    CGContextSetTextDrawingMode(context, kCGTextInvisible);
    CGContextShowTextAtPoint(context, startPos.x, startPos.y, upperTextString, [upperText length]);
    endPos = CGContextGetTextPosition(context);
    textSize = CGSizeMake(endPos.x - startPos.x, endPos.y - startPos.y);
    textOrigin = CGPointIntegral(origin.x + roundf((width - textSize.width) / 2.0f), origin.y + offset + 5.0f);
    CGContextSetTextDrawingMode(context, kCGTextFill);
    CGContextSetShadowWithColor(context, CGSizeMake(1.0f, 1.0f), 0.0f, blackColor);
    CGContextShowTextAtPoint(context, textOrigin.x, textOrigin.y, upperTextString, [upperText length]);
    
    CGContextRestoreGState(context);
    CGContextRestoreGState(context);
}

- (void)drawLayer:(CALayer *)layer inContext:(CGContextRef)context
{
    if ([layer isKindOfClass:[Atari800KeyLayer class]]) {
        Atari800KeyLayer *keyLayer = (Atari800KeyLayer *)layer;
        CGSize layerSize = layer.bounds.size;
        CGPoint origin = CGPointIntegral(0.0f, 0.0f);
        CGFloat width = layerSize.width;
        CGFloat height = layerSize.height - borderShadowOffset;
        switch (keyLayer.keyStyle) {
            case Atari800KeySimple:
                // Q W E R T Y etc.
                [self drawKeyInContext:context 
                              withFont:[NSString stringWithUTF8String:Atari800KeyboardFontFamily]
                              withSize:0.0f 
                              atOrigin:origin 
                             withWidth:width 
                             andHeight:height
                   withBackgroundColor:defaultKeyColor
                               andText:keyLayer.text];
                break;
            case Atari800KeyUpperLower:
                // e.g. number keys
                [self drawSplitKeyInContext:context 
                                   atOrigin:origin 
                                  withWidth:width 
                                  andHeight:height 
                               andUpperText:keyLayer.upperText 
                               andLowerText:keyLayer.text];
                break;
            case Atari800KeyThreePart:
                // TAB key
                [self drawThreePartKeyInContext:context 
                                       atOrigin:origin 
                                      withWidth:width 
                                      andHeight:height 
                               andUpperLeftText:keyLayer.upperLeftText 
                                   inSymbolFont:NO 
                              andUpperRightText:keyLayer.upperText 
                                   andLowerText:keyLayer.text];
                break;
            case Atari800KeyThreePartSymbol:
                // e.g. cursor arrows
                [self drawThreePartKeyInContext:context 
                                       atOrigin:origin 
                                      withWidth:width 
                                      andHeight:height 
                               andUpperLeftText:keyLayer.upperLeftText 
                                   inSymbolFont:YES
                              andUpperRightText:keyLayer.upperText 
                                   andLowerText:keyLayer.text];
                
                break;
            case Atari800KeyAtari:
                // Atari (inverse) key
                [self drawKeyInContext:context 
                              withFont:@"SFAtarianSystem"
                              withSize:32.0f 
                              atOrigin:origin 
                             withWidth:width 
                             andHeight:height 
                   withBackgroundColor:defaultKeyColor 
                               andText:@"*"];
                break;
            case Atari800KeyCtrl:
                // CTRL key
                [self drawKeyInContext:context 
                              withFont:[NSString stringWithUTF8String:Atari800KeyboardFontFamily]
                              withSize:0.0f 
                              atOrigin:origin 
                             withWidth:width 
                             andHeight:height
                   withBackgroundColor:yellowHighlightColor
                               andText:keyLayer.text];
                break;
            case Atari800KeyShift:
                // Shift keys
                [self drawKeyInContext:context 
                              withFont:[NSString stringWithUTF8String:Atari800KeyboardFontFamily]
                              withSize:0.0f 
                              atOrigin:origin 
                             withWidth:width 
                             andHeight:height
                   withBackgroundColor:orangeKeyColor
                               andText:keyLayer.text];
                break;
            default:
                break;
        }
    }
}

- (CALayer *)layerForKey:(NSString *)text withTag:(NSUInteger)tag andUpperLeftText:(NSString *)upperLeftText andUpperText:(NSString *)upperText withStyle:(Atari800KeyLayerStyle)keyStyle withWidth:(CGFloat)width andHeight:(CGFloat)height
{
    Atari800KeyLayer *keyLayer = [Atari800KeyLayer layer];
    if (width == 0.0f)
        width = defaultKeyWidth;
    if (height == 0.0f)
        height = defaultKeyHeight;
    
    height += borderShadowOffset;
    keyLayer.bounds = CGRectIntegral(CGRectMake(0.0f, 0.0f, width, height));
    keyLayer.delegate = self;
    keyLayer.tag = tag;
    keyLayer.keyStyle = keyStyle;
    keyLayer.text = text;
    keyLayer.upperLeftText = upperLeftText;
    keyLayer.upperText = upperText;
    keyLayer.magnificationFilter = kCAFilterTrilinear;
    keyLayer.minificationFilter = kCAFilterTrilinear;
    keyLayer.rasterizationScale = [[UIScreen mainScreen] scale];
    ///keyLayer.anchorPoint = CGPointMake(0.5f, 0.5f);
    keyLayer.contentsGravity = kCAGravityCenter;
    keyLayer.masksToBounds = NO;
    [keyLayer setNeedsDisplay];
    
    return keyLayer;
}

- (void)addKeyLayersToLayer:(CALayer *)layer
{
    
    CGFloat leftMargin = 33.0f;
    CGFloat leftInset = 12.0f;
    
    CGFloat xpos = leftMargin;
    CGFloat ypos = floorf(defaultKeyHeight / 2.0f + 8.0f);
    
    /*
     
     -------------------------
     Atari Keyboard Scan Codes
     -------------------------
     
     -    | $00     $01     $02     $03     $04     $05     $06     $07     $08     $09     $0A     $0B     $0C     $0D     $0E     $0F
     -----------------------------------------------------------------------------------------------------------------------------------   
     $00  | L       J       ;       F1      F2      K       +       *       O       N/A     P       U       CR      I       -       =
     $10  | V       Help    C       F3      F4      B       X       Z       4       N/A     3       6       Esc     5       2       1
     $20  | ,       Spc     .       N       N/A     M       /       Inv     R       N/A     E       Y       Tab     T       W       Q
     $30  | 9       N/A     0       7       BS      8       <       >       F       H       D       N/A     Caps    G       S       A
     
     */
    NSArray *keyURow0 = [NSArray arrayWithObjects:@"",       @"!",      @"\"",     @"#",      @"$",      @"%",      @"&",      @"'",      @"@",      @"(",      @")",      @"CLEAR",  @"INSERT", @"DELETE", @"",      nil];
    NSArray *keysRow0 = [NSArray arrayWithObjects:@"ESC",    @"1",      @"2",      @"3",      @"4",      @"5",      @"6",      @"7",      @"8",      @"9",      @"0",      @"<",      @">",      @"BACK S", @"BREAK", nil];
    NSArray *keyCode0 = [NSArray arrayWithObjects:@(0x1C), @(0x1f), @(0x1E), @(0x1A), @(0x18), @(0x1D), @(0x1B), @(0x33), @(0x35), @(0x30), @(0x32), @(0x36), @(0x37), @(0x34), @(AKEY_BREAK), nil];
    NSArray *keyLRow1 = [NSArray arrayWithObjects:@"",       @"",       @"",       @"",       @"",       @"",       @"",       @"",       @"",       @"",       @"⬆",     @"⬇", nil];
    NSArray *keyURow1 = [NSArray arrayWithObjects:@"",       @"",       @"",       @"",       @"",       @"",       @"",       @"",       @"",       @"",       @"_",      @"|", nil];
    NSArray *keysRow1 = [NSArray arrayWithObjects:@"Q",      @"W",      @"E",      @"R",      @"T",      @"Y",      @"U",      @"I",      @"O",      @"P",      @"-",      @"=", nil];
    NSArray *keyCode1 = [NSArray arrayWithObjects:@(0x2F), @(0x2E), @(0x2A), @(0x28), @(0x2D), @(0x2B), @(0x0B), @(0x0D), @(0x08), @(0x0A), @(0x0E), @(0x0F), nil];
    NSArray *keyLRow2 = [NSArray arrayWithObjects:@"",       @"",       @"",       @"",       @"",       @"",       @"",       @"",       @"",       @"",       @"⬅",     @"➡",      @"", nil];
    NSArray *keyURow2 = [NSArray arrayWithObjects:@"",       @"",       @"",       @"",       @"",       @"",       @"",       @"",       @"",       @":",      @"\\",     @"^",      @"CAPS", nil];
    NSArray *keysRow2 = [NSArray arrayWithObjects:@"A",      @"S",      @"D",      @"F",      @"G",      @"J",      @"H",      @"K",      @"L",      @";",      @"+",      @"*",      @"LOWR", nil];
    NSArray *keyCode2 = [NSArray arrayWithObjects:@(0x3F), @(0x3E), @(0x3A), @(0x38), @(0x3D), @(0x01), @(0x39), @(0x05), @(0x00), @(0x02), @(0x06), @(0x07), @(0x3C), nil];
    NSArray *keyURow3 = [NSArray arrayWithObjects:@"",       @"",       @"",       @"",       @"",       @"",       @"",       @"[",      @"]",      @"?", nil];
    NSArray *keysRow3 = [NSArray arrayWithObjects:@"Z",      @"X",      @"C",      @"V",      @"B",      @"N",      @"M",      @",",      @".",      @"/", nil];
    NSArray *keyCode3 = [NSArray arrayWithObjects:@(0x17), @(0x16), @(0x12), @(0x10), @(0x15), @(0x23), @(0x25), @(0x20), @(0x22), @(0x26), nil];

    NSInteger keyIndex = 0;
    
    CALayer *newLayer;
    
    for (NSString *key in keysRow0) {
        if ([[keyURow0 objectAtIndex:keyIndex] length] > 0) {
            newLayer = [self layerForKey:key 
                                 withTag:[[keyCode0 objectAtIndex:keyIndex] integerValue] 
                        andUpperLeftText:nil 
                            andUpperText:[keyURow0 objectAtIndex:keyIndex] 
                               withStyle:Atari800KeyUpperLower 
                               withWidth:defaultKeyWidth 
                               andHeight:defaultKeyHeight];
        }
        else {
            newLayer = [self layerForKey:key 
                                 withTag:[[keyCode0 objectAtIndex:keyIndex] integerValue] 
                        andUpperLeftText:nil 
                            andUpperText:[keyURow0 objectAtIndex:keyIndex] 
                               withStyle:Atari800KeySimple 
                               withWidth:defaultKeyWidth 
                               andHeight:defaultKeyHeight];
        }
        newLayer.position = CGPointIntegral(xpos, ypos);
        [layer addSublayer:newLayer];
        keyIndex ++;
        xpos += defaultKeyWidth;
        xpos += 10.0f;
    }
    
    CGFloat rightPos = xpos - 10.0f;
    
    xpos = leftMargin;
    ypos += defaultKeyHeight + 8.0f;
    
    CGFloat tabKeyWidth = roundf(defaultKeyWidth * 1.5f + 6.0f);
    
    newLayer = [self layerForKey:@"TAB" 
                         withTag:0x2C 
                andUpperLeftText:@"CLR" 
                    andUpperText:@"SET" 
                       withStyle:Atari800KeyThreePart 
                       withWidth:tabKeyWidth 
                       andHeight:defaultKeyHeight];
    newLayer.position = CGPointIntegral(xpos + CenterOffset(newLayer.bounds.size.width), ypos);
    [layer addSublayer:newLayer];
    
    xpos += tabKeyWidth;
    xpos += 10.0f;
    
    keyIndex = 0;
    for (NSString *key in keysRow1) {
        NSInteger tag = [[keyCode1 objectAtIndex:keyIndex] integerValue];
        if ([[keyLRow1 objectAtIndex:keyIndex] length] > 0) {
            newLayer = [self layerForKey:key 
                                 withTag:tag 
                        andUpperLeftText:[keyLRow1 objectAtIndex:keyIndex] 
                            andUpperText:[keyURow1 objectAtIndex:keyIndex] 
                               withStyle:Atari800KeyThreePartSymbol 
                               withWidth:defaultKeyWidth 
                               andHeight:defaultKeyHeight];
        }
        else if ([[keyURow1 objectAtIndex:keyIndex] length] > 0) {
            newLayer = [self layerForKey:key 
                                 withTag:tag
                        andUpperLeftText:nil 
                            andUpperText:[keyURow1 objectAtIndex:keyIndex] 
                               withStyle:Atari800KeyUpperLower 
                               withWidth:defaultKeyWidth 
                               andHeight:defaultKeyHeight];
        }
        else {
            newLayer = [self layerForKey:key 
                                 withTag:tag 
                        andUpperLeftText:nil 
                            andUpperText:nil 
                               withStyle:Atari800KeySimple 
                               withWidth:defaultKeyWidth 
                               andHeight:defaultKeyHeight];
        }
        newLayer.position = CGPointIntegral(xpos, ypos);
        [layer addSublayer:newLayer];
        keyIndex++;
        xpos += defaultKeyWidth;
        xpos += 10.0f;
    }
    
    // Return key
    newLayer = [self layerForKey:@"RETURN" 
                         withTag:0x0C 
                andUpperLeftText:nil 
                    andUpperText:nil 
                       withStyle:Atari800KeySimple 
                       withWidth:rightPos - xpos 
                       andHeight:defaultKeyHeight];
    newLayer.position = CGPointIntegral(xpos + CenterOffset(newLayer.bounds.size.width), ypos);
    [layer addSublayer:newLayer];
    
    xpos = leftMargin + leftInset;
    ypos += defaultKeyHeight + 8.0f;
    
    newLayer = [self layerForKey:@"CTRL" 
                         withTag:Atari800KeyCodeCtrl 
                andUpperLeftText:nil 
                    andUpperText:nil 
                       withStyle:Atari800KeyCtrl 
                       withWidth:tabKeyWidth 
                       andHeight:defaultKeyHeight];
    
    newLayer.position = CGPointIntegral(xpos + CenterOffset(newLayer.bounds.size.width), ypos);
    [layer addSublayer:newLayer];
    
    xpos += tabKeyWidth;
    xpos += 10.0f;
    keyIndex = 0;
    
    for (NSString *key in keysRow2) {
        NSInteger tag = [[keyCode2 objectAtIndex:keyIndex] integerValue];
        
        if ([[keyLRow2 objectAtIndex:keyIndex] length] > 0) {
            newLayer = [self layerForKey:key 
                                 withTag:tag 
                        andUpperLeftText:[keyLRow2 objectAtIndex:keyIndex] 
                            andUpperText:[keyURow2 objectAtIndex:keyIndex] 
                               withStyle:Atari800KeyThreePartSymbol 
                               withWidth:defaultKeyWidth 
                               andHeight:defaultKeyHeight];
        }
        else if ([[keyURow2 objectAtIndex:keyIndex] length] > 0) {
            newLayer = [self layerForKey:key 
                                 withTag:tag 
                        andUpperLeftText:nil 
                            andUpperText:[keyURow2 objectAtIndex:keyIndex] 
                               withStyle:Atari800KeyUpperLower 
                               withWidth:defaultKeyWidth 
                               andHeight:defaultKeyHeight];
        }
        else {
            newLayer = [self layerForKey:key 
                                 withTag:tag 
                        andUpperLeftText:nil 
                            andUpperText:nil 
                               withStyle:Atari800KeySimple 
                               withWidth:defaultKeyWidth 
                               andHeight:defaultKeyHeight];
        }
        newLayer.position = CGPointIntegral(xpos, ypos);
        [layer addSublayer:newLayer];
        keyIndex++;
        xpos += defaultKeyWidth;
        xpos += 10.0f;
    }
    
    CGFloat rightSide = xpos - 10.0f;
    
    
    xpos = leftMargin + leftInset;
    ypos += defaultKeyHeight + 8.0f;
    keyIndex = 0;
    
    newLayer = [self layerForKey:@"SHIFT" 
                         withTag:Atari800KeyCodeShift
                andUpperLeftText:nil 
                    andUpperText:nil 
                       withStyle:Atari800KeyShift 
                       withWidth:defaultKeyWidth * 2.0f + 8.0f 
                       andHeight:defaultKeyHeight];
    newLayer.position = CGPointIntegral(xpos + CenterOffset(newLayer.bounds.size.width), ypos);
    [layer addSublayer:newLayer];
    
    xpos += defaultKeyWidth * 2.0f + 8.0f;
    xpos += 10.0f;
    
    for (NSString *key in keysRow3) {
        NSInteger tag = [[keyCode3 objectAtIndex:keyIndex] integerValue];
        
        if ([[keyURow3 objectAtIndex:keyIndex] length] > 0) {
            
            newLayer = [self layerForKey:key 
                                 withTag:tag 
                        andUpperLeftText:nil 
                            andUpperText:[keyURow3 objectAtIndex:keyIndex] 
                               withStyle:Atari800KeyUpperLower 
                               withWidth:defaultKeyWidth 
                               andHeight:defaultKeyHeight];
        }
        else {
            newLayer = [self layerForKey:key 
                                 withTag:tag 
                        andUpperLeftText:nil 
                            andUpperText:nil 
                               withStyle:Atari800KeySimple 
                               withWidth:defaultKeyWidth 
                               andHeight:defaultKeyHeight];
        }
        newLayer.position = CGPointIntegral(xpos, ypos);
        [layer addSublayer:newLayer];
        keyIndex++;
        xpos += defaultKeyWidth;
        xpos += 10.0f;
    }
    
    newLayer = [self layerForKey:@"*" 
                         withTag:0x27
                andUpperLeftText:nil 
                    andUpperText:nil 
                       withStyle:Atari800KeyAtari 
                       withWidth:defaultKeyWidth 
                       andHeight:defaultKeyHeight];
    newLayer.position = CGPointIntegral(xpos, ypos);
    [layer addSublayer:newLayer];
    
    xpos += defaultKeyWidth;
    xpos += 10.0f;
    
    newLayer = [self layerForKey:@"SHIFT"
                         withTag:Atari800KeyCodeShift
                andUpperLeftText:nil 
                    andUpperText:nil 
                       withStyle:Atari800KeyShift 
                       withWidth:rightSide - xpos 
                       andHeight:defaultKeyHeight];
    newLayer.position = CGPointIntegral(xpos + CenterOffset(newLayer.bounds.size.width), ypos);
    [layer addSublayer:newLayer];
    
    // Space bar
    xpos = leftMargin + defaultKeyWidth * 3.75f;
    ypos += defaultKeyHeight + 8.0f;
    
    newLayer = [self layerForKey:@"" 
                         withTag:0x21
                andUpperLeftText:nil 
                    andUpperText:nil 
                       withStyle:Atari800KeySimple 
                       withWidth:defaultKeyWidth * 10.75f 
                       andHeight:defaultKeyHeight];
    
    newLayer.position = CGPointIntegral(xpos + CenterOffset(newLayer.bounds.size.width), ypos);
    [layer addSublayer:newLayer];
}

@end
