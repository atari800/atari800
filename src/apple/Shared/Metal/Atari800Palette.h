@import Foundation;

@interface Atari800Palette : NSObject

+ (NSData *)paletteWithURL:(NSURL *)url error:(NSError **)error;
+ (NSData *)paletteWithName:(NSString *)name error:(NSError **)error;
+ (NSData *)float4BufferDataFromPaletteData:(NSData *)paletteData error:(NSError **)error;
+ (NSData *)rgbaPaletteFromRGBPalette:(NSData *)paletteData error:(NSError **)error;

@end
