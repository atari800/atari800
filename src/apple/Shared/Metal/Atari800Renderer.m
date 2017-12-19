@import simd;
@import MetalKit;
@import AVFoundation;

#import "Atari800Renderer.h"
#import "Atari800Palette.h"
#import "Atari800ShaderTypes.h"

@interface Atari800Renderer() {
    id<MTLDevice> _device;
    id<MTLRenderPipelineState> _pipelineState;
    id<MTLCommandQueue> _commandQueue;
    id<MTLBuffer> _vertices;
    NSUInteger _numVertices;
    vector_uint2 _viewportSize;
    id<MTLBuffer> _screen;
    vector_uint2 _screenSize;
    id<MTLBuffer> _palette;
}

@end

@implementation Atari800Renderer

- (nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)view widthInPixels:(NSInteger)width heightInScanLines:(NSInteger)height
{
    self = [super init];
    
    if (self) {
        
        _device = view.device;

        // Set up a simple MTLBuffer with our vertices which include texture coordinates
        /*
         * Short - 320x240
         * Normal -  336x240
         * Full - 384x240
         */

        [self updateVertices:view.drawableSize];
        
        _screenSize.x = (unsigned int)width;
        _screenSize.y = (unsigned int)height;
        
        NSData *screen = [self setupScreenForWidthInPixels:width
                                         heightInScanLines:height];
        
        _screen = [_device newBufferWithBytes:[screen bytes]
                                       length:[screen length]
                                      options:MTLResourceStorageModeShared];
        
        NSData *paletteData = [Atari800Palette paletteWithName:@"jakub"
                                                           error:nil];
        NSData *bufferData = [Atari800Palette float4BufferDataFromPaletteData:paletteData
                                                                    error:nil];
        
        _palette = [_device newBufferWithBytes:[bufferData bytes]
                                        length:[bufferData length]
                                       options:MTLResourceStorageModeShared];
        
        NSBundle *bundle = [NSBundle bundleForClass:NSClassFromString(@"Atari800Renderer")];
        id<MTLLibrary> defaultLibrary = [_device newDefaultLibraryWithBundle:bundle
                                                                       error:nil];

        id<MTLFunction> vertexFunction = [defaultLibrary newFunctionWithName:@"vertexShader"];
        id<MTLFunction> fragmentFunction = [defaultLibrary newFunctionWithName:@"atari800BasicShader"];

        MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineStateDescriptor.label = @"Atari Drawing Pipeline";
        pipelineStateDescriptor.vertexFunction = vertexFunction;
        pipelineStateDescriptor.fragmentFunction = fragmentFunction;
        pipelineStateDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;

        NSError *error = NULL;
        _pipelineState = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor
                                                                 error:&error];
        if (!_pipelineState) {
            
            NSLog(@"Failed to created pipeline state, error %@", error);
        }

        // Create the command queue
        _commandQueue = [_device newCommandQueue];
    }

    return self;
}

- (uint32_t *)screen
{
    return (uint32_t *)[_screen contents];
}

- (NSData *)setupScreenForWidthInPixels:(NSInteger)width heightInScanLines:(NSInteger)height
{
    NSInteger screenLength = width * height * sizeof(uint32_t);
    NSMutableData *screen = [NSMutableData dataWithLength:screenLength];
    
    return screen;
}

- (void)updateVertices:(CGSize)size
{
    CGRect bounds = CGRectMake(0.0f, 0.0f, size.width, size.height);
    CGRect aspectRect = AVMakeRectWithAspectRatioInsideRect(CGSizeMake(_screenSize.x, _screenSize.y), bounds);
    CGSize aspectSize = aspectRect.size;
    
    float w = floorf(aspectSize.width / 2.0);
    float h = floorf(aspectSize.height / 2.0);
    
    Atari800Vertex quadVertices[] =
    {
        // Pixel positions, Texture coordinates
        { {  w,  -h },  { 1.f, 1.f } },
        { { -w,  -h },  { 0.f, 1.f } },
        { { -w,   h },  { 0.f, 0.f } },
        
        { {  w,  -h },  { 1.f, 1.f } },
        { { -w,   h },  { 0.f, 0.f } },
        { {  w,   h },  { 1.f, 0.f } },
    };
    
    _vertices = [_device newBufferWithBytes:quadVertices
                                     length:sizeof(quadVertices)
                                    options:MTLResourceStorageModeShared];
    
    _numVertices = sizeof(quadVertices) / sizeof(Atari800Vertex);
}

/// Called whenever view changes orientation or is resized
- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    // Save the size of the drawable as we'll pass these
    //   values to our vertex shader when we draw
    _viewportSize.x = size.width;
    _viewportSize.y = size.height;
    
    [self updateVertices:size];
}

/// Called whenever the view needs to render a frame
- (void)drawInMTKView:(nonnull MTKView *)view
{
    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    commandBuffer.label = @"RenderAtariScreen";

    MTLRenderPassDescriptor *renderPassDescriptor = view.currentRenderPassDescriptor;

    if (renderPassDescriptor != nil) {
        
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        renderEncoder.label = @"RenderAtariScreen";

        [renderEncoder setViewport:(MTLViewport) {0.0, 0.0, _viewportSize.x, _viewportSize.y, -1.0, 1.0 } ];

        [renderEncoder setRenderPipelineState:_pipelineState];

        [renderEncoder setVertexBuffer:_vertices
                                offset:0
                               atIndex:Atari800VertexInputIndexVertices];

        [renderEncoder setVertexBytes:&_viewportSize
                               length:sizeof(_viewportSize)
                              atIndex:Atari800VertexInputIndexViewportSize];

        [renderEncoder setFragmentBuffer:_palette
                                  offset:0
                                 atIndex:Atari800FragmentInputIndexPalette];
        
        [renderEncoder setFragmentBuffer:_screen
                                  offset:0
                                 atIndex:Atari800FragmentInputIndexScreen];
        
        [renderEncoder setFragmentBytes:&_screenSize
                                 length:sizeof(_screenSize)
                                atIndex:Atari800FragmentInputIndexScreenBounds];
        
        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                          vertexStart:0
                          vertexCount:_numVertices];

        [renderEncoder endEncoding];

        [commandBuffer presentDrawable:view.currentDrawable];
    }

    [commandBuffer commit];
}

@end

