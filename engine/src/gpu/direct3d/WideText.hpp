#pragma once

#include <string>
#include <string_view>

namespace haylen::gpu::direct3d {

// Converts the UTF-8 names of the engine into the UTF-16 text that the debug names and markers of Direct3D take, and back. An instance keeps its buffer, which grows to the longest text it converted and then never allocates.
class WideText final {
  public:
    [[nodiscard]] static std::wstring toWide(std::string_view text);
    [[nodiscard]] static std::string toUtf8(std::wstring_view text);

    // The text as UTF-16 ending in a null character, valid until the next conversion.
    [[nodiscard]] const wchar_t* convert(std::string_view text);

  private:
    std::wstring buffer;
};

} // namespace haylen::gpu::direct3d
