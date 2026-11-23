#include "haylen/debug/Inspector.hpp"

#include <algorithm>

namespace haylen::debug {

std::vector<std::string> Inspector::splitPath(std::string_view path) {
    std::vector<std::string> keys;
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t end = std::min(path.find('.', start), path.size());
        if (end > start) {
            keys.emplace_back(path.substr(start, end - start));
        }

        start = end + 1;
    }

    return keys;
}

std::string Inspector::joinPath(std::span<const std::string> path) {
    std::string joined;
    for (const std::string& key : path) {
        appendKey(joined, key);
    }

    return joined;
}

void Inspector::appendKey(std::string& path, std::string_view key) {
    if (!path.empty()) {
        path += '.';
    }

    path += key;
}

} // namespace haylen::debug
