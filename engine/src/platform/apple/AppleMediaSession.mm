#import "platform/apple/AppleMediaSession.hpp"

#include <TargetConditionals.h>
#include <dlfcn.h>

#include <array>
#include <utility>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

// The classes and methods of MediaPlayer that the runtime calls, with `id` in place of the types of the framework.
@protocol HaylenNowPlayingInfoCenter <NSObject>
+ (id)defaultCenter;
- (void)setNowPlayingInfo:(NSDictionary*)info;
@optional
- (void)setPlaybackState:(NSUInteger)state;
@end

@protocol HaylenRemoteCommand <NSObject>
- (id)addTargetWithHandler:(NSInteger (^)(id event))handler;
- (void)setEnabled:(BOOL)enabled;
@end

@protocol HaylenRemoteCommandCenter <NSObject>
+ (id)sharedCommandCenter;
- (id)playCommand;
- (id)pauseCommand;
- (id)togglePlayPauseCommand;
- (id)stopCommand;
- (id)nextTrackCommand;
- (id)previousTrackCommand;
- (id)changePlaybackPositionCommand;
@end

@protocol HaylenPositionEvent <NSObject>
- (NSTimeInterval)positionTime;
@end

@protocol HaylenMediaItemArtwork <NSObject>
- (id)initWithBoundsSize:(CGSize)size requestHandler:(id (^)(CGSize size))handler;
@end

namespace haylen::platform {

void* AppleMediaSession::library = nullptr;
bool AppleMediaSession::registered = false;

bool AppleMediaSession::load() {
    if (library == nullptr) {
        library = dlopen("/System/Library/Frameworks/MediaPlayer.framework/MediaPlayer", RTLD_LAZY | RTLD_LOCAL);
    }
    return library != nullptr && NSClassFromString(@"MPNowPlayingInfoCenter") != nil;
}

// The commands answer with the status `success`, 0, and a seek carries the position the person picked.
void AppleMediaSession::enableCommands(MediaSession::Commands commands) {
    id<HaylenRemoteCommandCenter> center = [(Class<HaylenRemoteCommandCenter>)NSClassFromString(@"MPRemoteCommandCenter") sharedCommandCenter];
    const std::array<std::pair<id, MediaSession::Command>, MediaSession::kCommandCount> list{{
        {[center playCommand], MediaSession::Command::Play},
        {[center pauseCommand], MediaSession::Command::Pause},
        {[center togglePlayPauseCommand], MediaSession::Command::TogglePlayPause},
        {[center stopCommand], MediaSession::Command::Stop},
        {[center nextTrackCommand], MediaSession::Command::Next},
        {[center previousTrackCommand], MediaSession::Command::Previous},
        {[center changePlaybackPositionCommand], MediaSession::Command::Seek},
    }};
    for (const auto& [command, kind] : list) {
        id<HaylenRemoteCommand> remote = command;
        if (!registered) {
            const MediaSession::Command received = kind;
            // clang-format off
            [remote addTargetWithHandler:^NSInteger(id event) {
                const double position = received == MediaSession::Command::Seek && [event respondsToSelector:@selector(positionTime)] ? [(id<HaylenPositionEvent>)event positionTime] : 0.0;
                MediaSession::receive(received, position);
                return 0;
            }];
            // clang-format on
        }
        [remote setEnabled:(commands & MediaSession::bit(kind)) != 0];
    }
    registered = true;
}

id AppleMediaSession::makeInfo(const MediaSession::NowPlaying& value) {
    NSMutableDictionary* info = [NSMutableDictionary dictionary];
    info[@"title"] = [NSString stringWithUTF8String:value.title.c_str()];
    info[@"artist"] = [NSString stringWithUTF8String:value.artist.c_str()];
    info[@"albumTitle"] = [NSString stringWithUTF8String:value.album.c_str()];
    if (value.duration > 0.0) {
        info[@"playbackDuration"] = @(value.duration);
    }
    info[@"MPNowPlayingInfoPropertyElapsedPlaybackTime"] = @(value.position);
    info[@"MPNowPlayingInfoPropertyPlaybackRate"] = @(value.playing ? 1.0 : 0.0);
    if (!value.artwork.empty()) {
        NSData* data = [NSData dataWithBytes:value.artwork.data() length:value.artwork.size()];
#if TARGET_OS_OSX
        NSImage* image = [[NSImage alloc] initWithData:data];
#else
        UIImage* image = [UIImage imageWithData:data];
#endif
        if (image != nil) {
            // clang-format off
            info[@"artwork"] = [(id<HaylenMediaItemArtwork>)[NSClassFromString(@"MPMediaItemArtwork") alloc] initWithBoundsSize:image.size requestHandler:^id(CGSize) { return image; }];
            // clang-format on
        }
    }
    return info;
}

// A Mac and Mac Catalyst follow the playback state too, which the media keys of the keyboard and the menu bar read.
void AppleMediaSession::update(const std::optional<MediaSession::NowPlaying>& value) {
    if (!load()) {
        return;
    }
    id<HaylenNowPlayingInfoCenter> center = [(Class<HaylenNowPlayingInfoCenter>)NSClassFromString(@"MPNowPlayingInfoCenter") defaultCenter];
    enableCommands(value ? value->commands : 0);
    [center setNowPlayingInfo:value ? makeInfo(*value) : nil];
    if ([center respondsToSelector:@selector(setPlaybackState:)]) {
        [center setPlaybackState:!value ? 3 : value->playing ? 1 : 2];
    }
}

} // namespace haylen::platform
