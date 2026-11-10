#pragma once

#include <CoreText/CoreText.h>

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "platform/SystemFont.hpp"

namespace haylen::platform {

// Finds the faces of the system that draw characters through the font cascade of CoreText, and hands over their data through the tables that CoreText copies, since the files of system fonts may lie where an app cannot read them.
class AppleFonts final {
  public:
    [[nodiscard]] static std::optional<SystemFont> find(char32_t codePoint);

    // Builds a font file from the tables of a face that `find` answered, which throws `std::runtime_error` for any other name or when the system copies none of its tables.
    [[nodiscard]] static std::vector<std::uint8_t> read(const SystemFont& font);

  private:
    // The font of the system that draws a box for every character no other font draws, which is no real fallback.
    static constexpr const char* kLastResort = "LastResort";

    // The descriptors of the faces that answered, by name, which stay while the process runs.
    static std::mutex& lock;
    static std::map<std::string, CTFontDescriptorRef, std::less<>>& descriptors;

    [[nodiscard]] static std::vector<std::uint32_t> copyTags(CTFontRef font);
    static void writeBigEndian(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value, std::size_t width);
};

} // namespace haylen::platform
