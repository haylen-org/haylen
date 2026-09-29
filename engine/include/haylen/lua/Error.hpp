#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::lua {

// An app error split into the parts an error screen and an editor show: the message, the script position Lua put in front of it and the stack of calls that led to it, innermost first. what() returns the text as Lua raised it. An error whose text has no script position takes the position of its innermost script frame, and errors raised outside scripts keep an empty file, line zero and no frames.
class Error final : public std::runtime_error {
  public:
    struct Frame {
        enum class Kind : std::uint8_t {
            Lua,
            C,
            Main,
        };

        // The chunk as Lua shows it, such as source/main.lua, or [C] for native functions.
        std::string source;
        int line = 0;
        // The function as Lua describes it, such as method 'update', local 'spawn' or main chunk.
        std::string function;
        Kind kind = Kind::Lua;

        // Returns source:line, or the source alone when the frame has no current line.
        [[nodiscard]] std::string getLocation() const;
    };

    explicit Error(const std::string& text, std::vector<Frame> stack = {});

    [[nodiscard]] const std::string& getMessage() const noexcept {
        return message;
    }
    [[nodiscard]] const std::string& getFile() const noexcept {
        return file;
    }
    [[nodiscard]] int getLine() const noexcept {
        return line;
    }
    [[nodiscard]] const std::vector<Frame>& getFrames() const noexcept {
        return frames;
    }

    // Renders the frames one per line as aligned location and function columns, separated by spaces and never by tab characters.
    [[nodiscard]] std::string getTraceback() const;
    [[nodiscard]] core::Json toJson() const;

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
    [[nodiscard]] static std::string_view getKindName(Frame::Kind kind) noexcept;

    std::string message;
    std::string file;
    int line = 0;
    std::vector<Frame> frames;
};

} // namespace haylen::lua
