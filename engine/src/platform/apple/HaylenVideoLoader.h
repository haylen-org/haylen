#pragma once

#import <AVFoundation/AVFoundation.h>

#include <memory>

#include "haylen/io/PackageReader.hpp"

NS_ASSUME_NONNULL_BEGIN

// Serves the bytes of a video file of the package to AVFoundation by range, so a video of a zip or of a protected release plays without a copy on disk. AVFoundation asks for the ranges on the queue of the loader.
@interface HaylenVideoLoader : NSObject <AVAssetResourceLoaderDelegate>

- (instancetype)initWithFile:(std::shared_ptr<haylen::io::PackageReader>)file extension:(NSString*)extension;

@end

NS_ASSUME_NONNULL_END
