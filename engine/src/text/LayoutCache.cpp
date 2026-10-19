#include "text/LayoutCache.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace haylen::text {

std::size_t LayoutCache::KeyHash::operator()(const Key& key) const noexcept {
    std::size_t hash = std::hash<std::string_view>{}(key.text);
    for (const std::size_t part : {std::hash<float>{}(key.size), std::hash<float>{}(key.maxWidth), std::hash<float>{}(key.maxHeight), std::hash<float>{}(key.minScale), std::hash<float>{}(key.lineSpacing), std::hash<std::string_view>{}(key.language), static_cast<std::size_t>(key.underline) << 9U | static_cast<std::size_t>(key.strike) << 8U | static_cast<std::size_t>(key.wrap) << 7U | static_cast<std::size_t>(key.align) << 4U | static_cast<std::size_t>(key.direction) << 2U | static_cast<std::size_t>(key.bold) << 1U | static_cast<std::size_t>(key.italic)}) {
        hash ^= part + std::size_t{0x9E3779B9U} + (hash << 6U) + (hash >> 2U);
    }
    return hash;
}

LayoutCache::Key LayoutCache::makeKey(std::string_view text, const Style& style) noexcept {
    return {.text = text, .size = style.size, .maxWidth = style.maxWidth, .maxHeight = style.maxHeight, .minScale = style.minScale, .wrap = style.wrap, .lineSpacing = style.lineSpacing, .align = style.align, .direction = style.direction, .language = style.language, .bold = style.bold, .italic = style.italic, .underline = style.underline, .strike = style.strike};
}

// The most recently used layouts stay, and the one used longest ago makes room for a new one. Lengths must be finite, since a key holding NaN never equals itself and could never be found or evicted.
std::shared_ptr<const Layout> LayoutCache::find(std::string_view text, const Style& style) {
    if (!std::isfinite(style.size) || !std::isfinite(style.maxWidth) || !std::isfinite(style.maxHeight) || !std::isfinite(style.minScale) || !std::isfinite(style.lineSpacing)) {
        throw std::invalid_argument("Text needs a finite size, maximum width, maximum height, minimum scale and line spacing.");
    }
    const auto found = index.find(makeKey(text, style));
    if (found == index.end()) {
        return nullptr;
    }
    entries.splice(entries.begin(), entries, found->second);
    return found->second->layout;
}

std::shared_ptr<const Layout> LayoutCache::store(std::string_view text, const Style& style, Layout built) {
    auto layout = std::make_shared<const Layout>(std::move(built));
    Entry& entry = entries.emplace_front(std::string(text), style.language, makeKey(text, style), layout);
    entry.key.text = entry.text;
    entry.key.language = entry.language;
    index.emplace(entry.key, entries.begin());
    if (entries.size() > kCapacity) {
        index.erase(entries.back().key);
        entries.pop_back();
    }
    return layout;
}

} // namespace haylen::text
