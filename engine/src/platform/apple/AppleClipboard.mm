#import "platform/apple/AppleClipboard.hpp"

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>

namespace haylen::platform {

void AppleClipboard::set(std::string_view text) {
    UIPasteboard.generalPasteboard.string = [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
}

std::string AppleClipboard::get() {
    NSString* text = UIPasteboard.generalPasteboard.string;
    return text != nil ? std::string(text.UTF8String) : std::string();
}

} // namespace haylen::platform
#endif
