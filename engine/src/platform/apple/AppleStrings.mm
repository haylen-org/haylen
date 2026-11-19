#import "platform/apple/AppleStrings.hpp"

#include <string>

#include "platform/TextHelper.hpp"

namespace haylen::platform {

NSString* AppleStrings::toNSString(std::string_view text) {
    const std::string valid = TextHelper::toValidUtf8(text);
    return [[NSString alloc] initWithBytes:valid.data() length:valid.size() encoding:NSUTF8StringEncoding];
}

} // namespace haylen::platform
