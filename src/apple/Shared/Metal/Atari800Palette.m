#import "Atari800Palette.h"
@import simd;

@implementation Atari800Palette

+ (NSData *)paletteWithURL:(NSURL *)url error:(NSError **)error
{
    NSData *data = [NSData dataWithContentsOfURL:url
                                         options:NSDataReadingMappedIfSafe
                                           error:error];
    return data;
}

+ (NSData *)paletteWithName:(NSString *)name error:(NSError *__autoreleasing *)error
{
    NSURL *paletteURL = [[NSBundle bundleForClass:NSClassFromString(@"Atari800Palette")] URLForResource:name
                                                                                          withExtension:@"act"];
    
    return [self paletteWithURL:paletteURL
                          error:error];
}

+ (NSData *)float4BufferDataFromPaletteData:(NSData *)paletteData error:(NSError **)error
{
    // Convert from RGB to RGBA float4
    NSInteger numberOfColors = [paletteData length] / 3;
    NSInteger rgbaLength = numberOfColors * sizeof(simd_float4);
    NSMutableData *rgbaData = [NSMutableData dataWithLength:rgbaLength];
    
    const uint8_t *rgbPointer = [paletteData bytes];
    simd_float4 *rgbaPointer = [rgbaData mutableBytes];
    
    for (int i = 0; i < numberOfColors; ++i) {
    
        for (int c = 0; c < 3; ++c) {
            
            (*rgbaPointer)[c] = (float)(*rgbPointer) / 255.0f;
            rgbPointer++;
        }
        
        (*rgbaPointer)[3] = 1.0;
        rgbaPointer++;
    }
    
    return rgbaData;
}

@end
