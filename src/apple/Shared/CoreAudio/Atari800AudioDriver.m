//
//  Atari800AudioDriver.m
//  Atari800EmulationCore-macOS
//
//  Created by Simon Lawrence on 19/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

#import "Atari800AudioDriver.h"
#import "sound.h"
#import "Atari800AudioBuffer.h"
@import CoreAudio;
@import AudioToolbox;

@interface Atari800AudioDriver() {
@public
    AudioUnit _outputUnit;
    BOOL _initialisedAudio;
    Atari800AudioBuffer *_buffer;
}

@end

const unsigned int bufferSize = 4096;
#define kOutputBus 0
#define SAMPLING_FREQUENCY_HZ 44100

@implementation Atari800AudioDriver

static Atari800AudioDriver *sharedDriver = nil;

- (instancetype)init
{
    self = [super init];
    
    if (self) {
    
        _buffer = [[Atari800AudioBuffer alloc] initWithCapacity:bufferSize];
    }
    
    sharedDriver = self;
    
    return self;
}

- (void)dealloc
{
    [self stopSound];
}

OSStatus RenderCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
{
    __unsafe_unretained Atari800AudioDriver *driver = (__bridge Atari800AudioDriver *)inRefCon;
    unsigned int framesize = inNumberFrames * sizeof(SInt16);
    
    if (Atari800AudioBufferDataSize(driver->_buffer) >= framesize) {
        Atari800AudioBufferRead(driver->_buffer, ioData->mBuffers[0].mData, framesize);
    }
    else {
        memset(ioData->mBuffers[0].mData, 0, framesize);
        putchar('+');
    }
    
    return noErr;
}

- (void)initialiseSound
{
    // Open the remote I/O
    AudioComponentDescription ioUnitDescription ;
    ioUnitDescription.componentType = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
    ioUnitDescription.componentSubType = kAudioUnitSubType_RemoteIO;
#else
    ioUnitDescription.componentSubType = kAudioUnitSubType_DefaultOutput;
#endif
    ioUnitDescription.componentFlags = 0;
    ioUnitDescription.componentFlagsMask = 0;
    ioUnitDescription.componentManufacturer = kAudioUnitManufacturer_Apple;
    
    AudioComponent outputComponent = AudioComponentFindNext(NULL, &ioUnitDescription);
    OSStatus err = AudioComponentInstanceNew(outputComponent, &_outputUnit);
    if (outputComponent == NULL) { printf ("OpenAComponent=%d\n", (int)err); }
    
    // Set up a callback function to generate output to the output unit
    AURenderCallbackStruct input;
    input.inputProc = RenderCallback;
    input.inputProcRefCon = (__bridge void *)self;
    
    err = AudioUnitSetProperty (_outputUnit,
                                kAudioUnitProperty_SetRenderCallback,
                                kAudioUnitScope_Input,
                                0,
                                &input,
                                sizeof(input));
    if (err) {
        printf ("AudioUnitSetProperty-CB=%d\n", (int)err);
    }
}

- (void)startSound
{
    OSStatus err = noErr;
    
    if (!_initialisedAudio) {
        
        AudioStreamBasicDescription outputStreamFormat;
        outputStreamFormat.mFormatID = kAudioFormatLinearPCM;
        outputStreamFormat.mSampleRate = SAMPLING_FREQUENCY_HZ;
        outputStreamFormat.mChannelsPerFrame = 1;
        outputStreamFormat.mBitsPerChannel = 16;
        outputStreamFormat.mBytesPerPacket = 2;
        outputStreamFormat.mBytesPerFrame = 2;
        outputStreamFormat.mFramesPerPacket = 1;
        outputStreamFormat.mFormatFlags =
        kLinearPCMFormatFlagIsSignedInteger |
        kLinearPCMFormatFlagIsPacked |
        kLinearPCMFormatFlagIsNonInterleaved
        | kLinearPCMFormatFlagIsBigEndian;
        
        //
        err = AudioUnitSetProperty (_outputUnit,
                                    kAudioUnitProperty_StreamFormat,
                                    kAudioUnitScope_Input,
                                    kOutputBus,
                                    &outputStreamFormat,
                                    sizeof(AudioStreamBasicDescription));
        if (err) {
            printf ("AudioUnitSetProperty-SF=%4.4s, %d\n", (char*)&err, (int)err);
            return;
        }
        
        UInt32 flag = 1;
        err = AudioUnitSetProperty(_outputUnit,
                                   kAudioOutputUnitProperty_EnableIO,
                                   kAudioUnitScope_Output,
                                   kOutputBus,
                                   &flag,
                                   sizeof(flag));
        err = AudioUnitInitialize(_outputUnit);
        if (err) {
            printf ("AudioUnitInitialize=%d\n", (int)err);
            return;
        }
        
        Float64 outSampleRate;
        UInt32 size = sizeof(Float64);
        err = AudioUnitGetProperty (_outputUnit,
                                    kAudioUnitProperty_SampleRate,
                                    kAudioUnitScope_Output,
                                    0,
                                    &outSampleRate,
                                    &size);
        if (err) {
            printf ("AudioUnitSetProperty-GF=%4.4s, %d\n", (char*)&err, (int)err);
            return;
        }
        
        Atari800AudioBufferClear(_buffer);
    }
    
    _initialisedAudio = YES;
    
    err = AudioOutputUnitStart(_outputUnit);
}

- (void) stopSound
{
    if (_initialisedAudio)
        AudioOutputUnitStop(_outputUnit);
    
    Atari800AudioBufferClear(_buffer);
}

@end

int PLATFORM_SoundSetup(Sound_setup_t *setup)
{
    [sharedDriver initialiseSound];

    return TRUE;
}

void PLATFORM_SoundExit(void)
{
    [sharedDriver stopSound];
}

void PLATFORM_SoundPause(void)
{
    
}

void PLATFORM_SoundContinue(void)
{
    [sharedDriver startSound];
}

unsigned int PLATFORM_SoundAvailable(void)
{
    if (!sharedDriver)
        return 0;
    
    return (unsigned int)Atari800AudioBufferFreeSpace(sharedDriver->_buffer);
}

void PLATFORM_SoundWrite(UBYTE const *buffer, unsigned int size)
{
    NSCAssert(size > 0, @"LOL WUT");
    Atari800AudioBufferWrite(sharedDriver->_buffer, buffer, size);
}
