#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/AppError.hpp"

namespace haylen::lua {

// Builds the app error of a Lua error from the text it raised and the stack of calls that led to it, innermost first.
class ErrorReader final {
  public:
    // Splits the position that Lua writes as `chunk:line:` in front of the message, or further in for errors raised through engine calls, from the text. An error whose text holds no position, such as an error value that is not a string or one raised with a level that leaves the position out, takes the position of its innermost script frame.
    [[nodiscard]] static core::AppError read(const std::string& text, std::vector<core::AppError::Frame> frames = {});

  private:
    // Where the script position lies in the text, from the chunk name to the space after the line.
    struct Position {
        std::size_t start = 0;
        std::size_t end = 0;
        std::string_view file;
        int line = 0;
    };

    [[nodiscard]] static std::optional<Position> findPosition(std::string_view text);
    [[nodiscard]] static bool isSeparator(char character) noexcept;
};

} // namespace haylen::lua
