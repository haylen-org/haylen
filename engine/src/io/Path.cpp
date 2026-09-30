#include "haylen/io/Path.hpp"

#include <stdexcept>
#include <vector>

namespace haylen::io {

std::string Path::normalize(std::string_view path) {
    if (path.starts_with('/') || path.starts_with('\\') || (path.size() > 1 && path[1] == ':')) {
        throw std::invalid_argument("The path \"" + std::string(path) + "\" must be relative.");
    }

    std::vector<std::string_view> segments;
    std::size_t start = 0;
    while (start <= path.size()) {
        std::size_t end = path.find_first_of("/\\", start);
        if (end == std::string_view::npos) {
            end = path.size();
        }

        const std::string_view segment = path.substr(start, end - start);
        if (segment == "..") {
            if (segments.empty()) {
                throw std::invalid_argument("The path \"" + std::string(path) + "\" must stay inside its root folder.");
            }
            segments.pop_back();
        } else if (!segment.empty() && segment != ".") {
            segments.push_back(segment);
        }
        start = end + 1;
    }

    std::string normalized;
    for (const std::string_view segment : segments) {
        if (!normalized.empty()) {
            normalized.push_back('/');
        }
        normalized.append(segment);
    }
    return normalized;
}

std::string Path::asset(std::string_view path) {
    const std::string relative = normalize(path);
    if (relative.empty()) {
        throw std::invalid_argument("An asset path cannot be empty.");
    }
    return std::string(kContentDirectory) + "/" + relative;
}

std::string Path::plugin(std::string_view id, std::string_view relative) {
    return join(join(kPluginsDirectory, id), relative);
}

std::string_view Path::extension(std::string_view path) noexcept {
    const std::size_t slash = path.find_last_of('/');
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) {
        return {};
    }
    return path.substr(dot);
}

std::string_view Path::directory(std::string_view path) noexcept {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string_view::npos ? std::string_view{} : path.substr(0, slash);
}

std::string Path::join(std::string_view folder, std::string_view relative) {
    if (folder.empty()) {
        return normalize(relative);
    }
    return normalize(std::string(folder) + "/" + std::string(relative));
}

bool Path::isInside(std::string_view path, std::string_view folder) noexcept {
    return folder.empty() || (path.size() > folder.size() && path.starts_with(folder) && path[folder.size()] == '/');
}

} // namespace haylen::io
