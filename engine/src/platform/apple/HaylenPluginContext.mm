#import "platform/apple/HaylenPluginContext+Runtime.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <utility>

#include "haylen/core/Log.hpp"
#include "haylen/platform/Permission.hpp"
#include "haylen/platform/PluginStreams.hpp"
#include "platform/NativeHost.hpp"
#include "platform/NativeWindow.hpp"
#import "platform/apple/AppleBridge.hpp"
#import "platform/apple/AppleScreens.hpp"
#import "platform/apple/HaylenAudioStream+Runtime.h"
#import "platform/apple/HaylenOverlay+Runtime.h"
#import "platform/apple/HaylenPermission+Runtime.h"
#import "platform/apple/HaylenRequirements+Runtime.h"
#import "platform/apple/HaylenVideoStream+Runtime.h"

#if !TARGET_OS_OSX
#import "platform/apple/ApplePresenter.hpp"
#endif

using haylen::core::Log;
using haylen::platform::AppleBridge;
using haylen::platform::AppleScreens;
using haylen::platform::AudioStream;
using haylen::platform::NativeHost;
using haylen::platform::Permission;
using haylen::platform::PluginStreams;
using haylen::platform::VideoStream;

@interface HaylenPluginContext ()

@property(nonatomic, readwrite, copy) NSString* identifier;
@property(nonatomic, readwrite, copy) NSDictionary<NSString*, id>* config;
@property(nonatomic, readwrite) HaylenOverlay* overlay;
@property(nonatomic, readwrite) HaylenRequirements* requirements;

@end

@implementation HaylenPluginContext {
    NSInteger covers;
    NSMutableDictionary<NSString*, HaylenPermission*>* permissions;
}

- (instancetype)initWithIdentifier:(NSString*)plugin config:(NSDictionary<NSString*, id>*)values {
    self = [super init];
    self.identifier = plugin;
    self.config = values;
    self.overlay = [[HaylenOverlay alloc] initWithIdentifier:plugin];
    self.requirements = [[HaylenRequirements alloc] initWithOwner:[NSString stringWithFormat:@"The plugin \"%@\"", plugin]];
    permissions = [NSMutableDictionary dictionary];
    return self;
}

#if TARGET_OS_OSX
- (NSWindow*)window {
    return haylen::platform::NativeWindow::isOpen() ? (__bridge NSWindow*)haylen::platform::NativeWindow::getHandle() : nil;
}
#else
- (UIViewController*)viewController {
    return haylen::platform::ApplePresenter::getTopmost();
}

- (UIWindowScene*)windowScene {
    return haylen::platform::NativeWindow::isOpen() ? ((__bridge UIWindow*)haylen::platform::NativeWindow::getHandle()).windowScene : nil;
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
    NativeHost::getNativeViews().coverApp();
}

- (void)uncoverApp {
    @synchronized(self) {
        if (covers == 0) {
            Log::error("The plugin \"{}\" uncovered the app without covering it first.", self.identifier.UTF8String);
            return;
        }
        --covers;
    }
    NativeHost::getNativeViews().uncoverApp();
}

- (void)closeCovers {
    NSInteger open = 0;
    @synchronized(self) {
        open = covers;
        covers = 0;
    }
    for (NSInteger index = 0; index < open; ++index) {
        NativeHost::getNativeViews().uncoverApp();
    }
}

- (void)registerPermission:(HaylenPermission*)permission {
    BOOL first = NO;
    @synchronized(self) {
        if (permissions[permission.name] != nil) {
            Log::error("The plugin \"{}\" registered the permission \"{}\" twice, and the first one stays.", self.identifier.UTF8String, permission.name.UTF8String);
            return;
        }
        first = permissions.count == 0;
        permissions[permission.name] = permission;
    }
    if (first) {
        [self registerPermissionHandler:@"permission" asking:NO];
        [self registerPermissionHandler:@"requestPermission" asking:YES];
    }
}

// The handlers answer `{"name": ...}` with the answer of the permission, and a name that the plugin did not register on this platform answers `unsupported`.
- (void)registerPermissionHandler:(NSString*)method asking:(BOOL)asking {
    __weak HaylenPluginContext* weakSelf = self;
    // clang-format off
    [self registerHandler:method handler:^(id params, HaylenReply reply) {
        HaylenPluginContext* context = weakSelf;
        id name = [params isKindOfClass:NSDictionary.class] ? params[@"name"] : nil;
        HaylenPermission* permission = [name isKindOfClass:NSString.class] ? [context findPermission:name] : nil;
        if (permission == nil) {
            reply(YES, @{@"state" : @"unsupported", @"message" : [NSString stringWithFormat:@"The plugin \"%@\" registers no permission \"%@\" on this platform.", context.identifier, name]});
            return;
        }
        [permission answerAsking:asking requirements:context.requirements completion:^(NSDictionary<NSString*, id>* answer) { reply(YES, answer); }];
    }];
    // clang-format on
}

- (nullable HaylenPermission*)findPermission:(NSString*)name {
    @synchronized(self) {
        return permissions[name];
    }
}

// A permission that the system tells nothing of until the app tries lets the plugin go on, and the completion runs on the main thread.
- (void)requirePermission:(NSString*)name completion:(void (^)(NSError* _Nullable error))completion {
    HaylenPermission* permission = [self findPermission:name];
    if (permission == nil) {
        NSString* message = [NSString stringWithFormat:@"The plugin \"%@\" requires the permission \"%@\", which it did not register with \"registerPermission\".", self.identifier, name];
        completion([NSError errorWithDomain:@"HaylenPermissions" code:0 userInfo:@{NSLocalizedDescriptionKey : message, @"message" : message, @"code" : @"unsupported"}]);
        return;
    }

    // clang-format off
    [permission answerAsking:YES requirements:self.requirements completion:^(NSDictionary<NSString*, id>* answer) {
        NSError* failure = [HaylenPluginContext failureOf:answer permission:permission];
        dispatch_async(dispatch_get_main_queue(), ^{ completion(failure); });
    }];
    // clang-format on
}

+ (nullable NSError*)failureOf:(NSDictionary<NSString*, id>*)answer permission:(HaylenPermission*)permission {
    NSString* state = answer[@"state"];
    if ([state isEqualToString:@"granted"] || [state isEqualToString:@"unknown"]) {
        return nil;
    }
    if (![state isEqualToString:@"unsupported"]) {
        const Permission::State answered = [state isEqualToString:@"prompt"] ? Permission::State::Prompt : Permission::State::Denied;
        NSString* message = @(Permission::refusal(answered, permission.capability.UTF8String, permission.name.UTF8String).c_str());
        return [NSError errorWithDomain:@"HaylenPermissions" code:0 userInfo:@{NSLocalizedDescriptionKey : message, @"message" : message, @"code" : @"denied"}];
    }

    NSString* message = answer[@"message"];
    NSMutableDictionary<NSString*, id>* failure = [NSMutableDictionary dictionaryWithDictionary:@{NSLocalizedDescriptionKey : message, @"message" : message, @"code" : @"unsupported"}];
    NSArray* missing = answer[@"missing"];
    if (missing.count > 0) {
        failure[@"data"] = @{@"missing" : missing};
    }
    return [NSError errorWithDomain:@"HaylenPermissions" code:0 userInfo:failure];
}

- (NSString*)qualify:(NSString*)name {
    return [NSString stringWithFormat:@"%@.%@", self.identifier, name];
}

@end
