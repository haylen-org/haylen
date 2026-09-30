#pragma once

#import "haylen/platform/apple/HaylenAudioStream.h"

#include <memory>

#include "haylen/platform/AudioStream.hpp"

NS_ASSUME_NONNULL_BEGIN

// The engine stream behind the audio stream that a plugin pushes into.
@interface HaylenAudioStream ()

- (instancetype)initWithStream:(std::shared_ptr<haylen::platform::AudioStream>)stream;

@end

NS_ASSUME_NONNULL_END
