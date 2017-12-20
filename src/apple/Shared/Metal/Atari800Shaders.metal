#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#import "Atari800ShaderTypes.h"

typedef struct
{
    float4 clipSpacePosition [[position]];
    float2 screenCoordinate;
} RasterizerData;

// Vertex Function
vertex RasterizerData vertexShader(uint vertexID [[ vertex_id ]],
                                   constant Atari800Vertex *vertexArray [[ buffer(Atari800VertexInputIndexVertices) ]],
                                   constant vector_uint2 *viewportSizePointer  [[ buffer(Atari800VertexInputIndexViewportSize) ]])

{
    RasterizerData out;

    float2 pixelSpacePosition = vertexArray[vertexID].position.xy;
    float2 viewportSize = float2(*viewportSizePointer);
    
    out.clipSpacePosition.xy = pixelSpacePosition / (viewportSize / 2.0);
    out.clipSpacePosition.z = 0.0;
    out.clipSpacePosition.w = 1.0;
    
    out.screenCoordinate = vertexArray[vertexID].screenCoordinate;
    
    return out;
}

// TODO: *Very* basic fragment function, need to add scan lines etc.
fragment float4 atari800BasicShader(RasterizerData in [[stage_in]],
                                    constant float4 *palette [[buffer(Atari800FragmentInputIndexPalette)]],
                                    constant uint8_t *screen [[buffer(Atari800FragmentInputIndexScreen)]],
                                    constant vector_uint2 *screenSizePointer [[buffer(Atari800FragmentInputIndexScreenBounds)]])
{
    float2 screenSize = float2(*screenSizePointer);
    float2 pos = in.screenCoordinate * screenSize;
    
    int2 pixel = int2(round(pos));
    
    uint8_t c = screen[pixel.x + pixel.y * (*screenSizePointer).x];
    
    return palette[c];
}

