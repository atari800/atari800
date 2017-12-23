//
//  Atari800QuartzRenderer.m
//  Atari800EmulationCore-iOS
//
//  Created by Rod Münch on 22/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800QuartzRenderer.h"
#import "Atari800Palette.h"
@import UIKit;
@import QuartzCore;

@interface Atari800QuartzRenderer() {
    
    size_t _screenLength;
    size_t _width;
    size_t _height;
    
    NSMutableData *_screen;
    NSMutableData *_imageData;
    
    CGImageRef _screenImage;
    CGDataProviderRef _imageProvider;
    CGColorSpaceRef _colorspace;
    NSData *_palette;
    
    __weak CALayer *_displayLayer;
    __weak CADisplayLink *_refresh;
}

@end

@implementation Atari800QuartzRenderer

const size_t Atari800QuartzBytesPerPixel = sizeof(uint32_t);
const size_t Atari800QuartzBitsPerPixel = 32;
const size_t Atari800QuartzBitsPerComponent = 8;

void Atari800QuartzReleaseData(void *info, const void *data, size_t size)
{
    // The data is released elsewhere
}

- (instancetype)init
{
#if TARGET_OS_SIMULATOR
    NSLog(@"Falling back to Quartz rendering for simulator.");
#else
    NSAssert(NO, @"Atari800QuartzRenderer is for simulator reendering only!");
#endif
    
    self = [super init];
    
    return self;
}

- (void)dealloc
{
    if (_refresh)
        [_refresh invalidate];
    
    if (_screenImage)
        CGImageRelease(_screenImage);
    
    if (_imageProvider)
        CGDataProviderRelease(_imageProvider);
    
    if (_colorspace)
        CGColorSpaceRelease(_colorspace);
}

- (void)setupScreenForWidthInPixels:(NSInteger)width heightInScanLines:(NSInteger)height
{
    _width = width;
    _height = height;
    _screenLength = width * height;
    
    NSMutableData *screen = [NSMutableData dataWithLength:_screenLength];
    
    _palette = [Atari800Palette rgbaPaletteFromRGBPalette:[Atari800Palette paletteWithName:@"jakub"
                                                                                             error:nil] error:nil];
    
    NSInteger screenImageSizeBytes = Atari800QuartzBytesPerPixel * width * height;
    _imageData = [[NSMutableData alloc] initWithCapacity:screenImageSizeBytes];
    _imageProvider = CGDataProviderCreateWithData(NULL, [_imageData mutableBytes], screenImageSizeBytes, Atari800QuartzReleaseData);
    _colorspace = CGColorSpaceCreateDeviceRGB();
    
    _screen = screen;
}

- (uint8_t *)screen
{
    
    return [_screen mutableBytes];
}

- (void)setupForView:(id)view widthInPixels:(NSInteger)width heightInScanLines:(NSInteger)height
{
    NSParameterAssert(view);
    NSAssert([view isKindOfClass:[UIView class]], @"Invalid view class.");
    
    UIView *uiView = (UIView *)view;
    
    [self setupScreenForWidthInPixels:width
                    heightInScanLines:height];
    
    CALayer *displayLayer = uiView.layer;
    
    [displayLayer setContentsGravity:kCAGravityResizeAspect];
    [displayLayer setMagnificationFilter:kCAFilterNearest];
    [displayLayer setMinificationFilter:kCAFilterNearest];
    
    NSMutableDictionary *layerActions = [NSMutableDictionary dictionaryWithObjectsAndKeys:
                                         [NSNull null], @"onOrderIn",
                                         [NSNull null], @"onOrderOut",
                                         [NSNull null], @"sublayers",
                                         [NSNull null], @"contents",
                                         [NSNull null], @"bounds",
                                         nil];
    [displayLayer setActions:layerActions];
    
    _displayLayer = displayLayer;
    
    [self render:nil];
    [self setupRefresh];
}

- (void)setupRefresh
{
    CADisplayLink *refresh = [CADisplayLink displayLinkWithTarget:self
                                                         selector:@selector(render:)];
    [refresh addToRunLoop:[NSRunLoop currentRunLoop]
                  forMode:NSRunLoopCommonModes];
}

- (void)render:(id)sender
{
    uint32_t *image = [_imageData mutableBytes];
    const uint8_t *screen = [_screen bytes];
    const uint32_t *palette = [_palette bytes];
    
    for (NSInteger i = 0; i < _screenLength; ++i) {
        
        image[i] = palette[screen[i]];
    }
    
    CGImageRef screenImage = CGImageCreate(_width, _height, Atari800QuartzBitsPerComponent, Atari800QuartzBitsPerPixel, _width * Atari800QuartzBytesPerPixel, _colorspace, (CGBitmapInfo)kCGImageAlphaNoneSkipLast, _imageProvider, NULL, NO, kCGRenderingIntentDefault);
    [_displayLayer setContents:(__bridge id)screenImage];
    CGImageRelease(screenImage);
}

@end
