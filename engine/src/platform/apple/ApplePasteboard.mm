#import "platform/apple/ApplePasteboard.hpp"

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

namespace haylen::platform {

// The types of the pasteboard read without a prompt, so a pasteboard that holds text and answers none tells that the person refused.
void ApplePasteboard::read(std::function<void(Permission::Answer answer, std::string text)> callback) {
    // clang-format off
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
#if TARGET_OS_OSX
        NSPasteboard* pasteboard = NSPasteboard.generalPasteboard;
        const BOOL holdsText = [pasteboard.types containsObject:NSPasteboardTypeString];
        NSString* text = holdsText ? [pasteboard stringForType:NSPasteboardTypeString] : nil;
#else
        UIPasteboard* pasteboard = UIPasteboard.generalPasteboard;
        const BOOL holdsText = pasteboard.hasStrings;
        NSString* text = holdsText ? pasteboard.string : nil;
#endif
        if (holdsText && text == nil) {
            callback({.state = Permission::State::Denied, .message = Permission::refusal(Permission::Kind::Clipboard)}, {});
            return;
        }
        callback({.state = Permission::State::Granted}, text != nil ? std::string(text.UTF8String) : std::string());
    });
    // clang-format on
}

} // namespace haylen::platform
