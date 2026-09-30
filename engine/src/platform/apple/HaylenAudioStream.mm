#import "platform/apple/HaylenAudioStream+Runtime.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "haylen/core/Log.hpp"

using haylen::platform::AudioStream;

@implementation HaylenAudioStream {
    std::shared_ptr<AudioStream> stream;
}

- (instancetype)initWithStream:(std::shared_ptr<AudioStream>)value {
    self = [super init];
    stream = std::move(value);
    return self;
}

- (NSInteger)sampleRate {
    return static_cast<NSInteger>(stream->getSampleRate());
}

- (NSInteger)channels {
    return static_cast<NSInteger>(stream->getChannels());
}

- (HaylenAudioStreamFormat)format {
    return stream->getFormat() == AudioStream::Format::Float32 ? HaylenAudioStreamFormatFloat32 : HaylenAudioStreamFormatInt16;
}

// The samples fill whole frames by their count, so only missing samples can go wrong.
- (NSInteger)pushSamples:(const void*)samples frames:(NSInteger)frames {
    if (frames <= 0) {
        return 0;
    }
    if (samples == nullptr) {
        haylen::core::Log::error("Audio frames need samples, so they were dropped.");
        return 0;
    }
    const std::size_t count = static_cast<std::size_t>(frames) * stream->getChannels();
    if (stream->getFormat() == AudioStream::Format::Float32) {
        return static_cast<NSInteger>(stream->push(std::span<const float>(static_cast<const float*>(samples), count)));
    }
    return static_cast<NSInteger>(stream->push(std::span<const std::int16_t>(static_cast<const std::int16_t*>(samples), count)));
}

@end
