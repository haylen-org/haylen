#pragma once

#import "haylen/platform/apple/HaylenVideoStream.h"

#include <memory>

#include "haylen/platform/VideoStream.hpp"

NS_ASSUME_NONNULL_BEGIN

// The engine stream behind the video stream that a plugin pushes into.
@interface HaylenVideoStream ()

- (instancetype)initWithStream:(std::shared_ptr<haylen::platform::VideoStream>)stream;

@end

NS_ASSUME_NONNULL_END
