#pragma once

#include <string>
#include <string_view>

namespace haylen::platform {

// Converts between the UTF-8 text of the engine and the UTF-16 text of Windows.
class WindowsText final {
  public:
    [[nodiscard]] static std::string toUtf8(std::wstring_view text);
    [[nodiscard]] static std::wstring toWide(std::string_view text);
};

} // namespace haylen::platform
