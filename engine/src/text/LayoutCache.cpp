#include "text/LayoutCache.hpp"

#include <utility>

namespace haylen::text {

std::size_t LayoutCache::KeyHash::operator()(const Key& key) const noexcept {
    std::size_t hash = std::hash<std::string>{}(key.text);
    for (const std::size_t part : {std::hash<float>{}(key.size), std::hash<float>{}(key.maxWidth), std::hash<float>{}(key.lineSpacing), std::hash<std::string>{}(key.language), static_cast<std::size_t>(key.align) << 4U | static_cast<std::size_t>(key.direction) << 2U | static_cast<std::size_t>(key.bold) << 1U | static_cast<std::size_t>(key.italic)}) {
        hash ^= part + std::size_t{0x9E3779B9U} + (hash << 6U) + (hash >> 2U);
    }
    return hash;
}

// The most recently used layouts stay, and the one used longest ago makes room for a new one.
std::shared_ptr<const TextLayout> LayoutCache::get(std::string_view text, const TextStyle& style, const Builder& build) {
    Key key{.text = std::string(text), .size = style.size, .maxWidth = style.maxWidth, .lineSpacing = style.lineSpacing, .align = style.align, .direction = style.direction, .language = style.language, .bold = style.bold, .italic = style.italic};
    if (const auto found = index.find(key); found != index.end()) {
        entries.splice(entries.begin(), entries, found->second);
        return found->second->layout;
    }

    auto layout = std::make_shared<const TextLayout>(build());
    entries.push_front({.key = key, .layout = layout});
    index.emplace(std::move(key), entries.begin());
    if (entries.size() > kCapacity) {
        index.erase(entries.back().key);
        entries.pop_back();
    }
    return layout;
}

} // namespace haylen::text
