#include "haylen/lua/Error.hpp"

#include <algorithm>
#include <regex>
#include <utility>

namespace haylen::lua {

std::string Error::Frame::getLocation() const {
    return line > 0 ? source + ":" + std::to_string(line) : source;
}

Error::Error(const std::string& text, std::vector<Frame> stack) : std::runtime_error(text), message(text), frames(std::move(stack)) {
    // Lua writes the position as chunk:line: in front of the message, and errors raised through engine calls may carry it further in. A line number never has more digits than an int holds.
    static const std::regex position(R"(([^\s:]+\.lua):(\d{1,9}): )");
    std::smatch match;
    if (std::regex_search(text, match, position)) {
        file = match[1].str();
        line = std::stoi(match[2].str());
        if (match.position(0) == 0) {
            message = text.substr(static_cast<std::size_t>(match.length(0)));
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
