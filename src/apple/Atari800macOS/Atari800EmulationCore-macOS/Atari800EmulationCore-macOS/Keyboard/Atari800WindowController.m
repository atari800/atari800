//
//  Atari800WindowController.m
//  Atari800EmulationCore-macOS
//
//  Created by Simon Lawrence on 25/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800WindowController.h"
#import "akey.h"

@interface Atari800WindowController() {
@private
    int _keycode;
    int _shiftKey;
    int _ctrlKey;
}

@end

@implementation Atari800WindowController

NS_INLINE int Atari800TranslateKeycode(unichar character)
{
    int result = AKEY_NONE;
    
    switch (character) {
            
        case 'a':
            result = AKEY_a;
            break;
        case 'b':
            result = AKEY_b;
            break;
        case 'c':
            result = AKEY_c;
            break;
        case 'd':
            result = AKEY_d;
            break;
        case 'e':
            result = AKEY_e;
            break;
        case 'f':
            result = AKEY_f;
            break;
        case 'g':
            result = AKEY_g;
            break;
        case 'h':
            result = AKEY_h;
            break;
        case 'i':
            result = AKEY_i;
            break;
        case 'j':
            result = AKEY_j;
            break;
        case 'k':
            result = AKEY_k;
            break;
        case 'l':
            result = AKEY_l;
            break;
        case 'm':
            result = AKEY_m;
            break;
        case 'n':
            result = AKEY_n;
            break;
        case 'o':
            result = AKEY_o;
            break;
        case 'p':
            result = AKEY_p;
            break;
        case 'q':
            result = AKEY_q;
            break;
        case 'r':
            result = AKEY_r;
            break;
        case 's':
            result = AKEY_s;
            break;
        case 't':
            result = AKEY_t;
            break;
        case 'u':
            result = AKEY_u;
            break;
        case 'v':
            result = AKEY_v;
            break;
        case 'w':
            result = AKEY_w;
            break;
        case 'x':
            result = AKEY_x;
            break;
        case 'y':
            result = AKEY_y;
            break;
        case 'z':
            result = AKEY_z;
            break;
    }
    
    return result;
}

- (void)keyDown:(NSEvent *)event
{
    NSString *key = event.charactersIgnoringModifiers;
    _keycode = Atari800TranslateKeycode([key characterAtIndex:0]);
}

- (void)keyUp:(NSEvent *)event
{
    _keycode = AKEY_NONE;
}

- (id)supplementalTargetForAction:(SEL)action sender:(id)sender
{
    return self.nextResponder;
}

- (void)flagsChanged:(NSEvent *)event
{
    // NSEventModifierFlagShift              = 1 << 17, // Set if Shift key is pressed.
    // NSEventModifierFlagControl
    NSEventModifierFlags flags = event.modifierFlags;
    _shiftKey = (flags & NSEventModifierFlagShift) > 0;
    _ctrlKey = (flags & NSEventModifierFlagControl) > 0;
}

- (Atari800KeyboardHandler)newKeyboardHandler
{
    __unsafe_unretained Atari800WindowController *uSelf = self;
    
    Atari800KeyboardHandler handler = ^(int *keycode, int *shiftKey, int *ctrlKey) {
        
        *keycode = uSelf->_keycode;
        *shiftKey = uSelf->_shiftKey;
        *ctrlKey = uSelf->_ctrlKey;
    };
    
    return handler;
}

@end
