#include "runtime/ErrorReader.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace haylen::lua {

haylen::core::AppError ErrorReader::read(const std::string& text, std::vector<haylen::core::AppError::Frame> frames) {
    haylen::core::AppError::Details details{.message = text, .frames = std::move(frames)};
    if (const std::optional<Position> position = findPosition(text)) {
        details.file = std::string(position->file);
        details.line = position->line;

        if (position->start == 0) {
            details.message = text.substr(position->end);
        }

        return {text, std::move(details)};
    }

    const auto script = std::ranges::find_if(details.frames, [](const haylen::core::AppError::Frame& frame) { return frame.kind != haylen::core::AppError::Frame::Kind::Native && frame.line > 0; });
    if (script != details.frames.end()) {
        details.file = script->source;
        details.line = script->line;
    }

    return {text, std::move(details)};
}

// Finds the first `name.lua:line:` whose chunk name holds no space or colon and whose line has at most the nine digits an int holds. Every name ends at the colon after it, so the scan stays linear in the length of the text.
std::optional<ErrorReader::Position> ErrorReader::findPosition(std::string_view text) {
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

bool ErrorReader::isSeparator(char character) noexcept {
    return character == ':' || std::isspace(static_cast<unsigned char>(character)) != 0;
}

} // namespace haylen::lua
