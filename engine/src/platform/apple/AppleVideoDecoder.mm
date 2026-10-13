#import "platform/apple/AppleVideoDecoder.hpp"

#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace haylen::platform {

// AVFoundation loads the tracks and the duration asynchronously, and the worker of the I/O pool that opens the video waits for them.
AppleVideoDecoder::AppleVideoDecoder(std::shared_ptr<io::PackageReader> file, std::string_view filePath) : path(filePath) {
    NSString* name = [[NSString alloc] initWithBytes:filePath.data() length:filePath.size() encoding:NSUTF8StringEncoding];
    loader = [[HaylenVideoLoader alloc] initWithFile:std::move(file) extension:name.pathExtension];
    queue = dispatch_queue_create("dev.haylen.video", DISPATCH_QUEUE_SERIAL);
    NSURLComponents* components = [[NSURLComponents alloc] init];
    components.scheme = @"haylen-video";
    components.host = @"package";
    components.path = [@"/" stringByAppendingString:name];
    asset = [AVURLAsset URLAssetWithURL:components.URL options:nil];
    [asset.resourceLoader setDelegate:loader queue:queue];

    dispatch_semaphore_t loaded = dispatch_semaphore_create(0);
    __block NSArray<AVAssetTrack*>* pictures = nil;
    __block NSArray<AVAssetTrack*>* sounds = nil;
    __block NSError* failure = nil;
    [asset loadTracksWithMediaType:AVMediaTypeVideo
                 completionHandler:^(NSArray<AVAssetTrack*>* tracks, NSError* error) {
                   pictures = tracks;
                   failure = error;
                   dispatch_semaphore_signal(loaded);
                 }];
    dispatch_semaphore_wait(loaded, DISPATCH_TIME_FOREVER);
    [asset loadTracksWithMediaType:AVMediaTypeAudio
                 completionHandler:^(NSArray<AVAssetTrack*>* tracks, NSError*) {
                   sounds = tracks;
                   dispatch_semaphore_signal(loaded);
                 }];
    dispatch_semaphore_wait(loaded, DISPATCH_TIME_FOREVER);
    if (failure != nil || pictures.count == 0) {
        throw std::runtime_error("The video \"" + path + "\" holds no picture that this platform plays" + (failure != nil ? ": " + std::string(failure.localizedDescription.UTF8String) : std::string()) + ".");
    }
    videoTrack = pictures.firstObject;
    audioTrack = sounds.firstObject;
    [asset loadValuesAsynchronouslyForKeys:@[ @"duration" ] completionHandler:^{ dispatch_semaphore_signal(loaded); }];
    dispatch_semaphore_wait(loaded, DISPATCH_TIME_FOREVER);
    [videoTrack loadValuesAsynchronouslyForKeys:@[ @"naturalSize" ] completionHandler:^{ dispatch_semaphore_signal(loaded); }];
    dispatch_semaphore_wait(loaded, DISPATCH_TIME_FOREVER);

    const CGSize size = videoTrack.naturalSize;
    info.duration = CMTimeGetSeconds(asset.duration);
    info.width = static_cast<int>(size.width);
    info.height = static_cast<int>(size.height);
    if (audioTrack != nil) {
        [audioTrack loadValuesAsynchronouslyForKeys:@[ @"formatDescriptions" ] completionHandler:^{ dispatch_semaphore_signal(loaded); }];
        dispatch_semaphore_wait(loaded, DISPATCH_TIME_FOREVER);
        const auto description = (__bridge CMAudioFormatDescriptionRef)audioTrack.formatDescriptions.firstObject;
        if (const AudioStreamBasicDescription* basic = description != nullptr ? CMAudioFormatDescriptionGetStreamBasicDescription(description) : nullptr) {
            info.sampleRate = static_cast<std::uint32_t>(basic->mSampleRate);
            info.channels = std::min<std::uint32_t>(basic->mChannelsPerFrame, 2);
        }
    }
    startReading(0.0);
}

AppleVideoDecoder::~AppleVideoDecoder() {
    [reader cancelReading];
}

// Pictures come as BGRA and sound as interleaved floats at the rate of the track, with at most two channels.
void AppleVideoDecoder::startReading(double seconds) {
    [reader cancelReading];
    NSError* error = nil;
    reader = [AVAssetReader assetReaderWithAsset:asset error:&error];
    if (reader == nil) {
        throw std::runtime_error("The video \"" + path + "\" could not be read: " + std::string(error.localizedDescription.UTF8String) + ".");
    }
    reader.timeRange = CMTimeRangeMake(CMTimeMakeWithSeconds(seconds, 90000), kCMTimePositiveInfinity);
    videoOutput = [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:videoTrack outputSettings:@{(id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)}];
    videoOutput.alwaysCopiesSampleData = NO;
    [reader addOutput:videoOutput];
    audioOutput = nil;
    if (info.channels > 0) {
        NSDictionary* settings = @{AVFormatIDKey : @(kAudioFormatLinearPCM), AVLinearPCMBitDepthKey : @32, AVLinearPCMIsFloatKey : @YES, AVLinearPCMIsNonInterleaved : @NO, AVLinearPCMIsBigEndianKey : @NO, AVSampleRateKey : @(info.sampleRate), AVNumberOfChannelsKey : @(info.channels)};
        audioOutput = [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:audioTrack outputSettings:settings];
        audioOutput.alwaysCopiesSampleData = NO;
        [reader addOutput:audioOutput];
    }
    if (![reader startReading]) {
        failReading();
    }
    soundTime = seconds;
    soundEnded = info.channels == 0;
}

void AppleVideoDecoder::failReading() const {
    NSString* reason = reader.error != nil ? reader.error.localizedDescription : @"AVFoundation gave no reason";
    throw std::runtime_error("The video \"" + path + "\" could not be decoded: " + std::string(reason.UTF8String) + ".");
}

// Samples without a picture, which some files hold, are skipped.
VideoDecoder::Read AppleVideoDecoder::readFrame(Frame& frame) {
    CMSampleBufferRef sample = nullptr;
    CVImageBufferRef image = nullptr;
    while (image == nullptr) {
        if (sample != nullptr) {
            CFRelease(sample);
        }
        sample = [videoOutput copyNextSampleBuffer];
        if (sample == nullptr) {
            if (reader.status == AVAssetReaderStatusFailed) {
                failReading();
            }
            return Read::End;
        }
        image = CMSampleBufferGetImageBuffer(sample);
    }
    CVPixelBufferLockBaseAddress(image, kCVPixelBufferLock_ReadOnly);
    const auto width = static_cast<int>(CVPixelBufferGetWidth(image));
    const auto height = static_cast<int>(CVPixelBufferGetHeight(image));
    const std::size_t stride = CVPixelBufferGetBytesPerRow(image);
    const auto* source = static_cast<const std::uint8_t*>(CVPixelBufferGetBaseAddress(image));
    frame.width = width;
    frame.height = height;
    frame.time = CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sample));
    frame.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
    for (int row = 0; row < height; ++row) {
        const std::uint8_t* from = source + static_cast<std::size_t>(row) * stride;
        std::uint8_t* to = frame.pixels.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(width) * 4U;
        for (int column = 0; column < width; ++column, from += 4, to += 4) {
            to[0] = from[2];
            to[1] = from[1];
            to[2] = from[0];
            to[3] = from[3];
        }
    }
    CVPixelBufferUnlockBaseAddress(image, kCVPixelBufferLock_ReadOnly);
    CFRelease(sample);
    return Read::Frame;
}

bool AppleVideoDecoder::readAudio(std::vector<float>& samples, double until) {
    while (!soundEnded && soundTime < until) {
        const CMSampleBufferRef sample = [audioOutput copyNextSampleBuffer];
        if (sample == nullptr) {
            if (reader.status == AVAssetReaderStatusFailed) {
                failReading();
            }
            soundEnded = true;
            break;
        }
        if (CMBlockBufferRef block = CMSampleBufferGetDataBuffer(sample)) {
            const std::size_t length = CMBlockBufferGetDataLength(block);
            const std::size_t start = samples.size();
            samples.resize(start + length / sizeof(float));
            CMBlockBufferCopyDataBytes(block, 0, (samples.size() - start) * sizeof(float), samples.data() + start);
        }
        soundTime = CMTimeGetSeconds(CMTimeAdd(CMSampleBufferGetPresentationTimeStamp(sample), CMSampleBufferGetDuration(sample)));
        CFRelease(sample);
    }
    return !soundEnded;
}

void AppleVideoDecoder::seek(double seconds) {
    startReading(seconds);
}

} // namespace haylen::platform
