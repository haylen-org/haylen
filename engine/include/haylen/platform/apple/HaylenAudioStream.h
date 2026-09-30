#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// The samples that native code pushes into an audio stream: 32-bit floats from -1 to 1, or 16-bit signed integers.
typedef NS_ENUM(NSInteger, HaylenAudioStreamFormat) {
    HaylenAudioStreamFormatFloat32,
    HaylenAudioStreamFormatInt16,
} NS_SWIFT_NAME(HaylenAudioStream.Format);

// An audio stream of a plugin, such as a microphone, a synthesized voice or decoded network audio, which the app plays through `handle:audioStream(name)` as a voice of `haylen.audio`. Native code writes interleaved samples into a lock-free ring from any thread, one thread at a time, and the voice resamples them to the mixer and plays silence where samples are missing. Streams belong to the process, so a stream stays valid for good.
@interface HaylenAudioStream : NSObject

@property(nonatomic, readonly) NSInteger sampleRate;
@property(nonatomic, readonly) NSInteger channels;
@property(nonatomic, readonly) HaylenAudioStreamFormat format;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

// Writes frames of interleaved samples in the format of the stream and returns how many frames fit, dropping the rest while the ring is full.
- (NSInteger)pushSamples:(const void*)samples frames:(NSInteger)frames NS_SWIFT_NAME(push(_:frames:));

@end

NS_ASSUME_NONNULL_END
