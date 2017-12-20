@import MetalKit;

@interface Atari800Renderer : NSObject<MTKViewDelegate>

@property (nonatomic, readonly) uint8_t * _Nullable screen;

- (nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)view widthInPixels:(NSInteger)width heightInScanLines:(NSInteger)height;

@end
