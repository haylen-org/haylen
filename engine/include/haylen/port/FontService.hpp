#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "haylen/platform/SystemFont.hpp"

namespace haylen::port {

// The fonts of the system, which draw the characters that no font of the app holds. A port registers it where its platform hands fonts of the system to apps, and the engine finds none on a platform without it. Both methods run on a worker of the I/O pool, one at a time.
class FontService {
  public:
    FontService() = default;
    FontService(const FontService&) = delete;
    FontService& operator=(const FontService&) = delete;
    virtual ~FontService() = default;

    // Finds the face of the system that draws the code point, or none where no face draws it or the face holds bitmaps alone.
    [[nodiscard]] virtual std::optional<platform::SystemFont> findSystemFont(char32_t codePoint) = 0;

    // Reads the font file of a face that `findSystemFont` found, and throws `std::runtime_error` when the system does not hand it over.
    [[nodiscard]] virtual std::vector<std::uint8_t> readSystemFont(const platform::SystemFont& font) = 0;
};

} // namespace haylen::port
