#import "platform/apple/HaylenPluginContext+Runtime.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <utility>

#include "haylen/core/Log.hpp"
#include "haylen/platform/PluginStreams.hpp"
#import "platform/apple/AppleBridge.hpp"
#import "platform/apple/AppleScreens.hpp"
#import "platform/apple/HaylenAudioStream+Runtime.h"
#import "platform/apple/HaylenOverlay+Runtime.h"
#import "platform/apple/HaylenVideoStream+Runtime.h"
#include "platform/sokol/SokolHost.hpp"
#include "sokol_app.h"

#if !TARGET_OS_OSX
#import "platform/apple/ApplePresenter.hpp"
#endif

using haylen::core::Log;
using haylen::platform::AppleBridge;
using haylen::platform::AppleScreens;
using haylen::platform::AudioStream;
using haylen::platform::PluginStreams;
using haylen::platform::SokolHost;
using haylen::platform::VideoStream;

@interface HaylenPluginContext ()

@property(nonatomic, readwrite, copy) NSString* identifier;
@property(nonatomic, readwrite, copy) NSDictionary<NSString*, id>* config;
@property(nonatomic, readwrite) HaylenOverlay* overlay;

@end

@implementation HaylenPluginContext {
    NSInteger covers;
}

- (instancetype)initWithIdentifier:(NSString*)plugin config:(NSDictionary<NSString*, id>*)values {
    self = [super init];
    self.identifier = plugin;
    self.config = values;
    self.overlay = [[HaylenOverlay alloc] initWithIdentifier:plugin];
    return self;
}

#if TARGET_OS_OSX
- (NSWindow*)window {
    return sapp_isvalid() ? (__bridge NSWindow*)sapp_macos_get_window() : nil;
}
#else
- (UIViewController*)viewController {
    return haylen::platform::ApplePresenter::getTopmost();
}

- (UIWindowScene*)windowScene {
    return sapp_isvalid() ? ((__bridge UIWindow*)sapp_ios_get_window()).windowScene : nil;
}
#endif

- (void)registerHandler:(NSString*)method handler:(HaylenHandler)handler {
    [HaylenBridge registerHandler:[self qualify:method] handler:handler];
}

- (void)registerCancellableHandler:(NSString*)method handler:(HaylenCancellableHandler)handler {
    [HaylenBridge registerCancellableHandler:[self qualify:method] handler:handler];
}

- (void)registerScreen:(NSString*)name handler:(HaylenScreenHandler)handler {
    AppleScreens::registerScreen([self qualify:name], handler);
}

- (HaylenVideoStream*)openVideoStream:(NSString*)name width:(NSInteger)width height:(NSInteger)height format:(HaylenVideoStreamFormat)format {
    try {
        if (format != HaylenVideoStreamFormatRGBA8 && format != HaylenVideoStreamFormatBGRA8) {
            throw std::invalid_argument("A video stream takes the format \"HaylenVideoStreamFormatRGBA8\" or \"HaylenVideoStreamFormatBGRA8\".");
        }
        if (!std::in_range<int>(width) || !std::in_range<int>(height)) {
            throw std::invalid_argument("The width and the height of a video stream fit a 32-bit integer.");
        }
        const VideoStream::Format pixels = format == HaylenVideoStreamFormatRGBA8 ? VideoStream::Format::Rgba8 : VideoStream::Format::Bgra8;
        return [[HaylenVideoStream alloc] initWithStream:PluginStreams::openVideo(self.identifier.UTF8String, name.UTF8String, pixels, static_cast<int>(width), static_cast<int>(height))];
    } catch (const std::exception& error) {
        Log::error("The plugin \"{}\" could not open the video stream \"{}\". {}", self.identifier.UTF8String, name.UTF8String, error.what());
        return nil;
    }
}

- (HaylenAudioStream*)openAudioStream:(NSString*)name sampleRate:(NSInteger)sampleRate channels:(NSInteger)channels format:(HaylenAudioStreamFormat)format capacity:(NSInteger)capacity {
    try {
        if (format != HaylenAudioStreamFormatFloat32 && format != HaylenAudioStreamFormatInt16) {
            throw std::invalid_argument("An audio stream takes the format \"HaylenAudioStreamFormatFloat32\" or \"HaylenAudioStreamFormatInt16\".");
        }
        if (sampleRate < 1 || channels < 1 || capacity < 1 || !std::in_range<std::uint32_t>(sampleRate) || !std::in_range<std::uint32_t>(channels)) {
            throw std::invalid_argument("An audio stream needs a sample rate, channels and room for at least one frame.");
        }
        const AudioStream::Format samples = format == HaylenAudioStreamFormatFloat32 ? AudioStream::Format::Float32 : AudioStream::Format::Int16;
        return [[HaylenAudioStream alloc] initWithStream:PluginStreams::openAudio(self.identifier.UTF8String, name.UTF8String, static_cast<std::uint32_t>(sampleRate), static_cast<std::uint32_t>(channels), samples, static_cast<std::size_t>(capacity))];
    } catch (const std::exception& error) {
        Log::error("The plugin \"{}\" could not open the audio stream \"{}\". {}", self.identifier.UTF8String, name.UTF8String, error.what());
        return nil;
    }
}

- (void)emit:(NSString*)event payload:(id)payload {
    AppleBridge::emit([self qualify : event], payload, {});
}

- (void)emitRetained:(NSString*)event payload:(id)payload {
    AppleBridge::emit([self qualify : event], payload, { .retain = true });
}

- (void)emit:(NSString*)event payload:(id)payload retain:(BOOL)retain batched:(BOOL)batched {
    AppleBridge::emit([self qualify : event], payload, { .retain = retain == YES, .batched = batched == YES });
}

- (void)coverApp {
    @synchronized(self) {
        ++covers;
    }
    SokolHost::getNativeViews().coverApp();
}

- (void)uncoverApp {
    @synchronized(self) {
        if (covers == 0) {
            Log::error("The plugin \"{}\" uncovered the app without covering it first.", self.identifier.UTF8String);
            return;
        }
        --covers;
    }
    SokolHost::getNativeViews().uncoverApp();
}

- (void)closeCovers {
    NSInteger open = 0;
    @synchronized(self) {
        open = covers;
        covers = 0;
    }
    for (NSInteger index = 0; index < open; ++index) {
        SokolHost::getNativeViews().uncoverApp();
    }
}

- (NSString*)qualify:(NSString*)name {
    return [NSString stringWithFormat:@"%@.%@", self.identifier, name];
}

@end
