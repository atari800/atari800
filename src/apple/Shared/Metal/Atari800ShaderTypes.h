#ifndef Atari800ShaderTypes_h
#define Atari800ShaderTypes_h

#include <simd/simd.h>

typedef enum Atari800VertexInputIndex
{
    Atari800VertexInputIndexVertices     = 0,
    Atari800VertexInputIndexViewportSize = 1,
} Atari800VertexInputIndex;

typedef enum Atari800FragmentInputIndex
{
    Atari800FragmentInputIndexPalette = 0,
    Atari800FragmentInputIndexScreen = 1,
    Atari800FragmentInputIndexScreenBounds = 2,
} Atari800FragmentInputIndex;

typedef struct
{
    // Positions in pixel space (i.e. a value of 100 indicates 100 pixels from the origin/center)
    vector_float2 position;
    // 2D screen coordinate 0.0 ... 1.0
    vector_float2 screenCoordinate;
} Atari800Vertex;

#endif /* Atari800ShaderTypes_h */
