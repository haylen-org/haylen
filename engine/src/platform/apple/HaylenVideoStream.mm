#import "platform/apple/HaylenVideoStream+Runtime.h"

#include <cstddef>
#include <exception>
#include <utility>

#include "haylen/core/Log.hpp"

using haylen::core::Log;
using haylen::platform::VideoStream;

@implementation HaylenVideoStream {
    std::shared_ptr<VideoStream> stream;
}

- (instancetype)initWithStream:(std::shared_ptr<VideoStream>)value {
    self = [super init];
    stream = std::move(value);
    return self;
}

- (HaylenVideoStreamFormat)format {
    return stream->getFormat() == VideoStream::Format::Rgba8 ? HaylenVideoStreamFormatRGBA8 : HaylenVideoStreamFormatBGRA8;
}

- (BOOL)pushPixels:(const void*)pixels width:(NSInteger)width height:(NSInteger)height stride:(NSInteger)stride timestamp:(NSTimeInterval)timestamp {
    if (pixels == nullptr || stride < 0 || !std::in_range<int>(width) || !std::in_range<int>(height)) {
        Log::error("A video frame needs pixels, a width and a height that fit a 32-bit integer and a stride of 0 or more, so it was dropped.");
        return NO;
    }
    try {
        stream->push(static_cast<const std::byte*>(pixels), static_cast<int>(width), static_cast<int>(height), static_cast<std::size_t>(stride), timestamp);
        return YES;
    } catch (const std::exception& error) {
        Log::error("A video frame was dropped. {}", error.what());
        return NO;
    }
}

- (BOOL)pushPixelBuffer:(CVPixelBufferRef)pixelBuffer timestamp:(NSTimeInterval)timestamp {
    const bool rgba = stream->getFormat() == VideoStream::Format::Rgba8;
    if (CVPixelBufferGetPixelFormatType(pixelBuffer) != (rgba ? kCVPixelFormatType_32RGBA : kCVPixelFormatType_32BGRA)) {
        Log::error("A video stream of \"{}\" pixels takes pixel buffers of \"{}\", so a frame of another pixel format was dropped.", rgba ? "RGBA8" : "BGRA8", rgba ? "kCVPixelFormatType_32RGBA" : "kCVPixelFormatType_32BGRA");
        return NO;
    }

    CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    const auto width = static_cast<NSInteger>(CVPixelBufferGetWidth(pixelBuffer));
    const auto height = static_cast<NSInteger>(CVPixelBufferGetHeight(pixelBuffer));
    const auto stride = static_cast<NSInteger>(CVPixelBufferGetBytesPerRow(pixelBuffer));
    const BOOL pushed = [self pushPixels:CVPixelBufferGetBaseAddress(pixelBuffer) width:width height:height stride:stride timestamp:timestamp];
    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    return pushed;
}

@end
