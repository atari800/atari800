//
//  Atari800WindowController.m
//  Atari800EmulationCore-macOS
//
//  Created by Rod Münch on 25/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800WindowController.h"
#import "akey.h"

const uint8_t AtariStickUp    = 0x01;
const uint8_t AtariStickDown  = 0x02;
const uint8_t AtariStickLeft  = 0x04;
const uint8_t AtariStickRight = 0x08;

const uint8_t AtariConsoleStart  = 0x01;
const uint8_t AtariConsoleSelect = 0x02;
const uint8_t AtariConsoleOption = 0x04;

const uint8_t AtariStickUpMask    = 0xFE;
const uint8_t AtariStickDownMask  = 0xFD;
const uint8_t AtariStickLeftMask  = 0xFB;
const uint8_t AtariStickRightMask = 0xF7;

const uint8_t AtariConsoleStartMask  = 0xFE;
const uint8_t AtariConsoleSelectMask = 0xFD;
const uint8_t AtariConsoleOptionMask = 0xFB;

@interface Atari800WindowController() {
@private
    int _keycode;
    int _shiftKey;
    int _ctrlKey;
    
    uint8_t _console;
    uint8_t _trig0;
    uint8_t _stick0;
}

@end

@implementation Atari800WindowController

NS_INLINE int Atari800TranslateKeycode(unichar character, uint8_t *stick0, uint8_t *console)
{
    int result = AKEY_NONE;
    
    /// NSLog(@"%x", character);
    
    switch (character) {
          
        case NSTabCharacter:
            result = AKEY_TAB;
            break;
            
        case NSBackspaceCharacter:
        case NSDeleteCharacter:
            result = AKEY_BACKSPACE;
            break;
        
        case NSCarriageReturnCharacter:
            result = AKEY_RETURN;
            break;
        
            /*
        case NSUpArrowFunctionKey:
            result = AKEY_UP;
            break;
        case NSDownArrowFunctionKey:
            result = AKEY_DOWN;
            break;
        case NSLeftArrowFunctionKey:
            result = AKEY_LEFT;
            break;
        case NSRightArrowFunctionKey:
            result = AKEY_RIGHT;
            break;
            */
        
        case NSUpArrowFunctionKey:
            *stick0 &= AtariStickUpMask;
            break;
        case NSDownArrowFunctionKey:
            *stick0 &= AtariStickDownMask;
            break;
        case NSLeftArrowFunctionKey:
            *stick0 &= AtariStickLeftMask;
            break;
        case NSRightArrowFunctionKey:
            *stick0 &= AtariStickRightMask;
            break;
          
        case NSF5FunctionKey:
            *console &= AtariConsoleStartMask;
            break;
        
        case NSF6FunctionKey:
            *console &= AtariConsoleSelectMask;
            break;
            
        case NSF7FunctionKey:
            *console &= AtariConsoleOptionMask;
            break;
        
        case 0x1B:
            result = AKEY_ESCAPE;
            break;
        
        case ' ':
            result = AKEY_SPACE;
            break;
            
        case '0':
            result = AKEY_0;
            break;
        case '1':
            result = AKEY_1;
            break;
        case '2':
            result = AKEY_2;
            break;
        case '3':
            result = AKEY_3;
            break;
        case '4':
            result = AKEY_4;
            break;
        case '5':
            result = AKEY_5;
            break;
        case '6':
            result = AKEY_6;
            break;
        case '7':
            result = AKEY_7;
            break;
        case '8':
            result = AKEY_8;
            break;
        case '9':
            result = AKEY_9;
            break;
            
        case '!':
            result = AKEY_EXCLAMATION;
            break;
        case '@':
            result = AKEY_AT;
            break;
        case '#':
            result = AKEY_HASH;
            break;
        case '$':
            result = AKEY_DOLLAR;
            break;
        case '%':
            result = AKEY_PERCENT;
            break;
        case '^':
            result = AKEY_CARET;
            break;
        case '&':
            result = AKEY_AMPERSAND;
            break;
        case '*':
            result = AKEY_ASTERISK;
            break;
        case '(':
            result = AKEY_PARENLEFT;
            break;
        case ')':
            result = AKEY_PARENRIGHT;
            break;
        case '_':
            result = AKEY_UNDERSCORE;
            break;
        case '+':
            result = AKEY_PLUS;
            break;
        case '=':
            result = AKEY_EQUAL;
            break;
        case '[':
            result = AKEY_BRACKETLEFT;
            break;
        case ']':
            result = AKEY_BRACKETRIGHT;
            break;
        case ':':
            result = AKEY_COLON;
            break;
        case ';':
            result = AKEY_SEMICOLON;
            break;
        case '\"':
            result = AKEY_DBLQUOTE;
            break;
        case '\'':
            result = AKEY_QUOTE;
            break;
        case '\\':
            result = AKEY_BACKSLASH;
            break;
        case '|':
            result = AKEY_BAR;
            break;
        case '<':
            result = AKEY_LESS;
            break;
        case '>':
            result = AKEY_GREATER;
            break;
        case ',':
            result = AKEY_COMMA;
            break;
        case '.':
            result = AKEY_FULLSTOP;
            break;
        case '?':
            result = AKEY_QUESTION;
            break;
        case '/':
            result = AKEY_SLASH;
            break;
            
        case 0xA7:
            result = AKEY_ATARI;
            break;
            
        case 'a':
        case 'A':
            result = AKEY_a;
            break;
        case 'b':
        case 'B':
            result = AKEY_b;
            break;
        case 'c':
        case 'C':
            result = AKEY_c;
            break;
        case 'd':
        case 'D':
            result = AKEY_d;
            break;
        case 'e':
        case 'E':
            result = AKEY_e;
            break;
        case 'f':
        case 'F':
            result = AKEY_f;
            break;
        case 'g':
        case 'G':
            result = AKEY_g;
            break;
        case 'h':
        case 'H':
            result = AKEY_h;
            break;
        case 'i':
        case 'I':
            result = AKEY_i;
            break;
        case 'j':
        case 'J':
            result = AKEY_j;
            break;
        case 'k':
        case 'K':
            result = AKEY_k;
            break;
        case 'l':
        case 'L':
            result = AKEY_l;
            break;
        case 'm':
        case 'M':
            result = AKEY_m;
            break;
        case 'n':
        case 'N':
            result = AKEY_n;
            break;
        case 'o':
        case 'O':
            result = AKEY_o;
            break;
        case 'p':
        case 'P':
            result = AKEY_p;
            break;
        case 'q':
        case 'Q':
            result = AKEY_q;
            break;
        case 'r':
        case 'R':
            result = AKEY_r;
            break;
        case 's':
        case 'S':
            result = AKEY_s;
            break;
        case 't':
        case 'T':
            result = AKEY_t;
            break;
        case 'u':
        case 'U':
            result = AKEY_u;
            break;
        case 'v':
        case 'V':
            result = AKEY_v;
            break;
        case 'w':
        case 'W':
            result = AKEY_w;
            break;
        case 'x':
        case 'X':
            result = AKEY_x;
            break;
        case 'y':
        case 'Y':
            result = AKEY_y;
            break;
        case 'z':
        case 'Z':
            result = AKEY_z;
            break;
    }
    
    return result;
}

- (void)awakeFromNib
{
    [super awakeFromNib];
    _stick0 = 0x0F;
    _trig0 = 0x01;
    _console = 0x0F;
}

- (void)keyDown:(NSEvent *)event
{
    NSString *key = event.charactersIgnoringModifiers;
    _keycode = Atari800TranslateKeycode([key characterAtIndex:0], &_stick0, &_console);
}

- (void)keyUp:(NSEvent *)event
{
    NSString *key = event.charactersIgnoringModifiers;
    unichar keyCode = [key characterAtIndex:0];
    
    switch (keyCode) {
            
        case NSUpArrowFunctionKey:
            _stick0 |= AtariStickUp;
            break;
        case NSDownArrowFunctionKey:
            _stick0 |= AtariStickDown;
            break;
        case NSLeftArrowFunctionKey:
            _stick0 |= AtariStickLeft;
            break;
        case NSRightArrowFunctionKey:
            _stick0 |= AtariStickRight;
            break;
        
        case NSF5FunctionKey:
            _console |= AtariConsoleStart;
            break;
            
        case NSF6FunctionKey:
            _console |= AtariConsoleSelect;
            break;
            
        case NSF7FunctionKey:
            _console |= AtariConsoleOption;
            break;
            
        default:
            _keycode = AKEY_NONE;
            break;
    }
}

- (void)cancelOperation:(id)sender
{
    _keycode = AKEY_ESCAPE;
}

- (id)supplementalTargetForAction:(SEL)action sender:(id)sender
{
    
    return self.nextResponder;
}

- (void)flagsChanged:(NSEvent *)event
{
    NSEventModifierFlags flags = event.modifierFlags;
    _shiftKey = (flags & NSEventModifierFlagShift) > 0;
    _ctrlKey = (flags & NSEventModifierFlagControl) > 0;
    _trig0 = ((flags & NSEventModifierFlagOption) > 0) ? 0x00: 0x01;
}

- (Atari800KeyboardHandler)newKeyboardHandler
{
    __unsafe_unretained Atari800WindowController *uSelf = self;
    
    Atari800KeyboardHandler handler = ^(int *keycode, int *shiftKey, int *ctrlKey, int *console) {
        
        *keycode = uSelf->_keycode;
        *shiftKey = uSelf->_shiftKey;
        *ctrlKey = uSelf->_ctrlKey;
        *console = uSelf->_console;
    };
    
    return handler;
}

- (Atari800PortHandler)newPortHandler
{
    __unsafe_unretained Atari800WindowController *uSelf = self;
    
    Atari800PortHandler handler = ^(int port, int *stick, int *trig) {
        
        if (port == 0) {
            *stick = (uSelf->_stick0 & 0xF);
            *trig = uSelf->_trig0;
        }
        else {
            *stick = 0x0F;
            *trig = 0x01;
        }
    };
    
    return handler;
}

@end
