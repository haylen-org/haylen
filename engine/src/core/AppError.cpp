#include "haylen/core/AppError.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace haylen::core {

std::string AppError::Frame::getLocation() const {
    return line > 0 ? source + ":" + std::to_string(line) : source;
}

AppError::AppError(const std::string& text) : std::runtime_error(text), message(text) {}

AppError::AppError(const std::string& text, Details details) : std::runtime_error(text), message(std::move(details.message)), file(std::move(details.file)), line(details.line), frames(std::move(details.frames)) {}

std::string AppError::getTraceback() const {
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

Json AppError::toJson() const {
    Json stack = Json::array();
    for (const Frame& frame : frames) {
        stack.push_back({{"source", frame.source}, {"line", frame.line}, {"function", frame.function}, {"kind", Frame::kKindNames[static_cast<std::size_t>(frame.kind)]}});
    }

    Json json{{"code", kCodeNames[static_cast<std::size_t>(code)]}, {"message", message}, {"file", file}, {"line", line}, {"traceback", getTraceback()}, {"frames", std::move(stack)}};
    if (!callback.empty()) {
        json["callback"] = callback;
    }
    return json;
}

} // namespace haylen::core
