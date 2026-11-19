#pragma once

#import <Foundation/Foundation.h>

#include <string_view>

namespace haylen::platform {

// Turns the text of the engine and of the app into strings of Foundation.
class AppleStrings final {
  public:
    // Foundation answers nil for bytes that are not UTF-8, which ends the app once a dictionary or an announcement holds it, so they become U+FFFD as they do for every system, which keeps the offsets of a selection in code points.
    [[nodiscard]] static NSString* toNSString(std::string_view text);
};

} // namespace haylen::platform
