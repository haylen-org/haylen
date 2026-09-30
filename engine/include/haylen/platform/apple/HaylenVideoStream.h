#pragma once

#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// The layout of the pixels that native code pushes into a video stream, four bytes per pixel, which the engine turns into `RGBA8`.
typedef NS_ENUM(NSInteger, HaylenVideoStreamFormat) {
    HaylenVideoStreamFormatRGBA8,
    HaylenVideoStreamFormatBGRA8,
} NS_SWIFT_NAME(HaylenVideoStream.Format);

// A video stream of a plugin, such as a camera or a video decoder, which the app draws through `handle:videoStream(name)`. Native code pushes frames from any thread, the stream keeps only the newest one, and the engine uploads it into the texture of the stream at the start of a frame, so the texture changes once per frame at most. A frame of another size resizes the texture. Streams belong to the process, so a stream stays valid for good and the app that starts after a restart receives its newest frame.
@interface HaylenVideoStream : NSObject

@property(nonatomic, readonly) HaylenVideoStreamFormat format;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

// Copies a frame of width by height pixels in the format of the stream, whose rows start stride bytes apart, with its timestamp in seconds. Returns `NO` and logs why for a size that is not positive or a stride shorter than a row.
- (BOOL)pushPixels:(const void*)pixels width:(NSInteger)width height:(NSInteger)height stride:(NSInteger)stride timestamp:(NSTimeInterval)timestamp NS_SWIFT_NAME(push(_:width:height:stride:timestamp:));

// Copies the frame of a pixel buffer, such as a frame of the camera or of a video decoder, whose pixel format is `kCVPixelFormatType_32BGRA` for a `BGRA8` stream and `kCVPixelFormatType_32RGBA` for an `RGBA8` stream. Returns `NO` and logs why for a pixel buffer of another format.
- (BOOL)pushPixelBuffer:(CVPixelBufferRef)pixelBuffer timestamp:(NSTimeInterval)timestamp NS_SWIFT_NAME(push(_:timestamp:));

@end

NS_ASSUME_NONNULL_END
