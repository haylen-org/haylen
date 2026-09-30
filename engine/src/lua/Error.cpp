#include "haylen/lua/Error.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace haylen::lua {

std::string Error::Frame::getLocation() const {
    return line > 0 ? source + ":" + std::to_string(line) : source;
}

Error::Error(const std::string& text, std::vector<Frame> stack) : std::runtime_error(text), message(text), frames(std::move(stack)) {
    // Lua writes the position as `chunk:line:` in front of the message, and errors raised through engine calls may carry it further in.
    if (const std::optional<Position> position = findPosition(text)) {
        file = std::string(position->file);
        line = position->line;
        if (position->start == 0) {
            message = text.substr(position->end);
        }
        return;
    }

    // Error values that are not strings, and errors raised with a level that leaves the position out, point at the innermost script frame.
    const auto script = std::ranges::find_if(frames, [](const Frame& frame) { return frame.kind != Frame::Kind::C && frame.line > 0; });
    if (script != frames.end()) {
        file = script->source;
        line = script->line;
    }
}

// Finds the first `name.lua:line:` whose chunk name holds no space or colon and whose line has at most the nine digits an int holds. Every name ends at the colon after it, so the scan stays linear in the length of the text.
std::optional<Error::Position> Error::findPosition(std::string_view text) {
    constexpr std::string_view marker = ".lua:";
    constexpr std::size_t maximumDigits = 9;
    for (std::size_t found = text.find(marker); found != std::string_view::npos; found = text.find(marker, found + 1)) {
        std::size_t start = found;
        while (start > 0 && !isSeparator(text[start - 1])) {
            --start;
        }

        const std::size_t digits = found + marker.size();
        std::size_t end = digits;
        while (end < text.size() && end - digits <= maximumDigits && std::isdigit(static_cast<unsigned char>(text[end])) != 0) {
            ++end;
        }
        const std::size_t count = end - digits;
        if (start == found || count == 0 || count > maximumDigits || text.substr(end, 2) != ": ") {
            continue;
        }
        return Position{.start = start, .end = end + 2, .file = text.substr(start, digits - 1 - start), .line = std::stoi(std::string(text.substr(digits, count)))};
    }
    return std::nullopt;
}

bool Error::isSeparator(char character) noexcept {
    return character == ':' || std::isspace(static_cast<unsigned char>(character)) != 0;
}

std::string Error::getTraceback() const {
    std::vector<std::string> locations;
    std::size_t width = 0;
    for (const Frame& frame : frames) {
        locations.push_back(frame.getLocation());
        width = std::max(width, locations.back().size());
    }

    std::string text;
    for (std::size_t index = 0; index < frames.size(); ++index) {
        if (index > 0) {
            text += '\n';
        }
        text += locations[index];
        text.append(width - locations[index].size() + 2, ' ');
        text += frames[index].function;
    }
    return text;
}

std::string_view Error::getKindName(Frame::Kind kind) noexcept {
    switch (kind) {
    case Frame::Kind::Lua:
        return "lua";
    case Frame::Kind::C:
        return "c";
    case Frame::Kind::Main:
        return "main";
    }
    return "lua";
}

core::Json Error::toJson() const {
    core::Json stack = core::Json::array();
    for (const Frame& frame : frames) {
        stack.push_back({{"source", frame.source}, {"line", frame.line}, {"function", frame.function}, {"kind", getKindName(frame.kind)}});
    }
    return {{"message", message}, {"file", file}, {"line", line}, {"traceback", getTraceback()}, {"frames", std::move(stack)}};
}

} // namespace haylen::lua
