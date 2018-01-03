//
//  Atari800Emulator+CartridgeHandling.m
//  Atari800EmulationCore
//
//  Created by Rod Münch on 20/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800Emulator+CartridgeHandling.h"
#import "Atari800EmulationThread.h"
#import "Atari800Dispatch.h"

#import "cartridge.h"
#import "util.h"

@implementation Atari800Emulator (CartridgeHandling)

int Atar800ValidateCartridgeImage(NSString *path, CARTRIDGE_image_t *cart)
{
    const char *filename = [path UTF8String];
    
    FILE *fp;
    int len;
    int type;
    UBYTE header[16];
    
    /* open file */
    fp = fopen(filename, "rb");
    if (fp == NULL)
        return CARTRIDGE_CANT_OPEN;
    /* check file length */
    len = Util_flen(fp);
    Util_rewind(fp);
    
    /* Guard against providing cart->filename as parameter. */
    if (cart->filename != filename)
    /* Save Filename for state save */
        strcpy(cart->filename, filename);
    
    /* if full kilobytes, assume it is raw image */
    if ((len & 0x3ff) == 0) {
        /* alloc memory and read data */
        cart->image = (UBYTE *) Util_malloc(len);
        if (fread(cart->image, 1, len, fp) < len) {
            NSLog(@"Error reading cartridge.\n");
        }
        fclose(fp);
        /* find cart type */
        cart->type = CARTRIDGE_NONE;
        len >>= 10;    /* number of kilobytes */
        cart->size = len;
        for (type = 1; type <= CARTRIDGE_LAST_SUPPORTED; type++)
            if (CARTRIDGE_kb[type] == len) {
                if (cart->type == CARTRIDGE_NONE) {
                    cart->type = type;
                } else {
                    /* more than one cartridge type of such length - user must select */
                    cart->type = CARTRIDGE_UNKNOWN;
                    return len;
                }
            }
        if (cart->type != CARTRIDGE_NONE) {
            //InitCartridge(cart);
            return 0;    /* ok */
        }
        free(cart->image);
        cart->image = NULL;
        return CARTRIDGE_BAD_FORMAT;
    }
    /* if not full kilobytes, assume it is CART file */
    if (fread(header, 1, 16, fp) < 16) {
        NSLog(@"Error reading cartridge.\n");
    }
    if ((header[0] == 'C') &&
        (header[1] == 'A') &&
        (header[2] == 'R') &&
        (header[3] == 'T')) {
        type = (header[4] << 24) |
        (header[5] << 16) |
        (header[6] << 8) |
        header[7];
        if (type >= 1 && type <= CARTRIDGE_LAST_SUPPORTED) {
            int checksum;
            int result;
            len = CARTRIDGE_kb[type] << 10;
            cart->size = CARTRIDGE_kb[type];
            /* alloc memory and read data */
            cart->image = (UBYTE *) Util_malloc(len);
            if (fread(cart->image, 1, len, fp) < len) {
                NSLog(@"Error reading cartridge.\n");
            }
            fclose(fp);
            checksum = (header[8] << 24) |
            (header[9] << 16) |
            (header[10] << 8) |
            header[11];
            cart->type = type;
            result = checksum == CARTRIDGE_Checksum(cart->image, len) ? 0 : CARTRIDGE_BAD_CHECKSUM;
            
            return result;
        }
    }
    fclose(fp);
    return CARTRIDGE_BAD_FORMAT;
}

- (NSDictionary<NSNumber *, NSString *> *)supportedCartridgeTypesForSize:(NSInteger)sizeKb
{
    static NSDictionary<NSNumber *, NSString *> *allCartridgeTypes = nil;
    
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        allCartridgeTypes = @{@(CARTRIDGE_STD_8): @CARTRIDGE_STD_8_DESC,
                              @(CARTRIDGE_STD_16): @CARTRIDGE_STD_16_DESC,
                              @(CARTRIDGE_OSS_034M_16): @CARTRIDGE_OSS_034M_16_DESC,
                              @(CARTRIDGE_5200_32): @CARTRIDGE_5200_32_DESC,
                              @(CARTRIDGE_DB_32): @CARTRIDGE_DB_32_DESC,
                              @(CARTRIDGE_5200_EE_16): @CARTRIDGE_5200_EE_16_DESC,
                              @(CARTRIDGE_5200_40): @CARTRIDGE_5200_40_DESC,
                              @(CARTRIDGE_WILL_64): @CARTRIDGE_WILL_64_DESC,
                              @(CARTRIDGE_EXP_64): @CARTRIDGE_EXP_64_DESC,
                              @(CARTRIDGE_DIAMOND_64): @CARTRIDGE_DIAMOND_64_DESC,
                              @(CARTRIDGE_SDX_64): @CARTRIDGE_SDX_64_DESC,
                              @(CARTRIDGE_XEGS_32): @CARTRIDGE_XEGS_32_DESC,
                              @(CARTRIDGE_XEGS_07_64): @CARTRIDGE_XEGS_07_64_DESC,
                              @(CARTRIDGE_XEGS_128): @CARTRIDGE_XEGS_128_DESC,
                              @(CARTRIDGE_OSS_M091_16): @CARTRIDGE_OSS_M091_16_DESC,
                              @(CARTRIDGE_5200_NS_16): @CARTRIDGE_5200_NS_16_DESC,
                              @(CARTRIDGE_ATRAX_DEC_128): @CARTRIDGE_ATRAX_DEC_128_DESC,
                              @(CARTRIDGE_BBSB_40): @CARTRIDGE_BBSB_40_DESC,
                              @(CARTRIDGE_5200_8): @CARTRIDGE_5200_8_DESC,
                              @(CARTRIDGE_5200_4): @CARTRIDGE_5200_4_DESC,
                              @(CARTRIDGE_RIGHT_8): @CARTRIDGE_RIGHT_8_DESC,
                              @(CARTRIDGE_WILL_32): @CARTRIDGE_WILL_32_DESC,
                              @(CARTRIDGE_XEGS_256): @CARTRIDGE_XEGS_256_DESC,
                              @(CARTRIDGE_XEGS_512): @CARTRIDGE_XEGS_512_DESC,
                              @(CARTRIDGE_XEGS_1024): @CARTRIDGE_XEGS_1024_DESC,
                              @(CARTRIDGE_MEGA_16): @CARTRIDGE_MEGA_16_DESC,
                              @(CARTRIDGE_MEGA_32): @CARTRIDGE_MEGA_32_DESC,
                              @(CARTRIDGE_MEGA_64): @CARTRIDGE_MEGA_64_DESC,
                              @(CARTRIDGE_MEGA_128): @CARTRIDGE_MEGA_128_DESC,
                              @(CARTRIDGE_MEGA_256): @CARTRIDGE_MEGA_256_DESC,
                              @(CARTRIDGE_MEGA_512): @CARTRIDGE_MEGA_512_DESC,
                              @(CARTRIDGE_MEGA_1024): @CARTRIDGE_MEGA_1024_DESC,
                              @(CARTRIDGE_SWXEGS_32): @CARTRIDGE_SWXEGS_32_DESC,
                              @(CARTRIDGE_SWXEGS_64): @CARTRIDGE_SWXEGS_64_DESC,
                              @(CARTRIDGE_SWXEGS_128): @CARTRIDGE_SWXEGS_128_DESC,
                              @(CARTRIDGE_SWXEGS_256): @CARTRIDGE_SWXEGS_256_DESC,
                              @(CARTRIDGE_SWXEGS_512): @CARTRIDGE_SWXEGS_512_DESC,
                              @(CARTRIDGE_SWXEGS_1024): @CARTRIDGE_SWXEGS_1024_DESC,
                              @(CARTRIDGE_PHOENIX_8): @CARTRIDGE_PHOENIX_8_DESC,
                              @(CARTRIDGE_BLIZZARD_16): @CARTRIDGE_BLIZZARD_16_DESC,
                              @(CARTRIDGE_ATMAX_128): @CARTRIDGE_ATMAX_128_DESC,
                              @(CARTRIDGE_ATMAX_1024): @CARTRIDGE_ATMAX_1024_DESC,
                              @(CARTRIDGE_SDX_128): @CARTRIDGE_SDX_128_DESC,
                              @(CARTRIDGE_OSS_8): @CARTRIDGE_OSS_8_DESC,
                              @(CARTRIDGE_OSS_043M_16): @CARTRIDGE_OSS_043M_16_DESC,
                              @(CARTRIDGE_BLIZZARD_4): @CARTRIDGE_BLIZZARD_4_DESC,
                              @(CARTRIDGE_AST_32): @CARTRIDGE_AST_32_DESC,
                              @(CARTRIDGE_ATRAX_SDX_64): @CARTRIDGE_ATRAX_SDX_64_DESC,
                              @(CARTRIDGE_ATRAX_SDX_128): @CARTRIDGE_ATRAX_SDX_128_DESC,
                              @(CARTRIDGE_TURBOSOFT_64): @CARTRIDGE_TURBOSOFT_64_DESC,
                              @(CARTRIDGE_TURBOSOFT_128): @CARTRIDGE_TURBOSOFT_128_DESC,
                              @(CARTRIDGE_ULTRACART_32): @CARTRIDGE_ULTRACART_32_DESC,
                              @(CARTRIDGE_LOW_BANK_8): @CARTRIDGE_LOW_BANK_8_DESC,
                              @(CARTRIDGE_SIC_128): @CARTRIDGE_SIC_128_DESC,
                              @(CARTRIDGE_SIC_256): @CARTRIDGE_SIC_256_DESC,
                              @(CARTRIDGE_SIC_512): @CARTRIDGE_SIC_512_DESC,
                              @(CARTRIDGE_STD_2): @CARTRIDGE_STD_2_DESC,
                              @(CARTRIDGE_STD_4): @CARTRIDGE_STD_4_DESC,
                              @(CARTRIDGE_RIGHT_4): @CARTRIDGE_RIGHT_4_DESC,
                              @(CARTRIDGE_BLIZZARD_32): @CARTRIDGE_BLIZZARD_32_DESC,
                              @(CARTRIDGE_MEGAMAX_2048): @CARTRIDGE_MEGAMAX_2048_DESC,
                              @(CARTRIDGE_THECART_128M): @CARTRIDGE_THECART_128M_DESC,
                              @(CARTRIDGE_MEGA_4096): @CARTRIDGE_MEGA_4096_DESC,
                              @(CARTRIDGE_MEGA_2048): @CARTRIDGE_MEGA_2048_DESC,
                              @(CARTRIDGE_THECART_32M): @CARTRIDGE_THECART_32M_DESC,
                              @(CARTRIDGE_THECART_64M): @CARTRIDGE_THECART_64M_DESC,
                              @(CARTRIDGE_XEGS_8F_64): @CARTRIDGE_XEGS_8F_64_DESC,
                              @(CARTRIDGE_ATRAX_128): @CARTRIDGE_ATRAX_128_DESC,
                              @(CARTRIDGE_ADAWLIAH_32): @CARTRIDGE_ADAWLIAH_32_DESC,
                              @(CARTRIDGE_ADAWLIAH_64): @CARTRIDGE_ADAWLIAH_64_DESC};
    });
    
    NSMutableDictionary<NSNumber *, NSString *> *applicableCartridgeTypes = [NSMutableDictionary dictionary];
    
    for (int i = 1; i <= CARTRIDGE_LAST_SUPPORTED; ++i)
        if (CARTRIDGE_kb[i] == sizeKb)
            applicableCartridgeTypes[@(i)] = allCartridgeTypes[@(i)];
    
    return applicableCartridgeTypes;
}

- (void)insertCartridge:(NSString *)path completion:(Atari800CompletionHandler)completion
{
    id<Atari800EmulatorDelegate> _delegate = self.delegate;
    
    if (_delegate && [_delegate respondsToSelector:@selector(emulator:didSelectCartridgeWithSize:filename:completion:)]) {
        
        CARTRIDGE_image_t cartridge;
        
        NSInteger sizeKb = Atar800ValidateCartridgeImage(path, &cartridge);
        
        if (sizeKb == 0) {
            
            Atari800UICommandEnqueue(Atari800CommandInsertCartridge, Atari800CommandParamLeftCartridge, cartridge.type, @[path]);
            return;
        }
        
        Atari800CartridgeSelectionHandler selection = ^(BOOL ok, NSInteger cartridgeType) {
            
            if (ok) {
                
                Atari800UICommandEnqueue(Atari800CommandInsertCartridge, Atari800CommandParamLeftCartridge, cartridgeType, @[path]);
            }
            
            completion(ok, nil);
        };
        
        AlwaysAsync(^{
            
            [_delegate emulator:self
     didSelectCartridgeWithSize:sizeKb
                       filename:[path lastPathComponent]
                     completion:selection];
        });
    }
}

@end
