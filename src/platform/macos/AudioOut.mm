
#import <TargetConditionals.h>
#if TARGET_OS_OSX
#import <AudioUnit/AudioUnit.h>
#import <AudioToolbox/AudioToolbox.h>
#endif
#import "platform/macos/AudioOut.h"
#include <vector>
#include <iostream>

struct AudioOutImpl {
#if TARGET_OS_OSX
    AudioComponentInstance unit = nullptr;
    AudioStreamBasicDescription asbd{};
#endif
    int frames = 0;
    int chans = 0;
    AudioOut::Render render;
    std::vector<float*> channelPtrs;
};

#if TARGET_OS_OSX
static OSStatus RenderCB(void* inRefCon,
                         AudioUnitRenderActionFlags* /*ioActionFlags*/,
                         const AudioTimeStamp* /*inTimeStamp*/,
                         UInt32 /*inBusNumber*/,
                         UInt32 inNumberFrames,
                         AudioBufferList* ioData) {
    static int counter = 0;

    AudioOutImpl* impl = (AudioOutImpl*)inRefCon;
    if (!impl) return noErr;
    // Build channel pointers from ioData
    impl->channelPtrs.clear();
    for (UInt32 i=0;i<ioData->mNumberBuffers;i++) {
        impl->channelPtrs.push_back((float*)ioData->mBuffers[i].mData);
    }
    AlchemyAudioBuffer buf{ impl->channelPtrs.data(), (int)ioData->mNumberBuffers, (int)inNumberFrames };
    if (impl->render) impl->render(buf);
    return noErr;
}
#endif

AudioOut::~AudioOut() { stop(); }

bool AudioOut::start(int sampleRate, int blockFrames, int numChannels, Render render) {
    if (impl_) return true;
    impl_ = new AudioOutImpl();
    auto* impl = (AudioOutImpl*)impl_;
    impl->render = render;
    impl->frames = blockFrames;
    impl->chans = numChannels;

#if TARGET_OS_OSX
    AudioComponentDescription desc{};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (!comp) return false;
    if (AudioComponentInstanceNew(comp, &impl->unit) != noErr) return false;

    // Non-interleaved float32
    impl->asbd.mSampleRate = sampleRate;
    impl->asbd.mFormatID = kAudioFormatLinearPCM;
    impl->asbd.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    impl->asbd.mFramesPerPacket = 1;
    impl->asbd.mChannelsPerFrame = numChannels;
    impl->asbd.mBytesPerFrame = sizeof(float);
    impl->asbd.mBytesPerPacket = sizeof(float);
    impl->asbd.mBitsPerChannel = 8 * sizeof(float);

    if (AudioUnitSetProperty(impl->unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &impl->asbd, sizeof(impl->asbd)) != noErr) return false;

    AURenderCallbackStruct cb{};
    cb.inputProc = RenderCB;
    cb.inputProcRefCon = impl;
    if (AudioUnitSetProperty(impl->unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof(cb)) != noErr) return false;

    if (AudioUnitInitialize(impl->unit) != noErr) return false;
    if (AudioOutputUnitStart(impl->unit) != noErr) return false;
    return true;
#else
    // Non-macOS: not implemented
    (void)sampleRate; (void)blockFrames; (void)numChannels; (void)render;
    return false;
#endif
}

void AudioOut::stop() {
    if (!impl_) return;
    auto* impl = (AudioOutImpl*)impl_;
#if TARGET_OS_OSX
    if (impl->unit) {
        AudioOutputUnitStop(impl->unit);
        AudioUnitUninitialize(impl->unit);
        AudioComponentInstanceDispose(impl->unit);
        impl->unit = nullptr;
    }
#endif
    delete impl;
    impl_ = nullptr;
}
