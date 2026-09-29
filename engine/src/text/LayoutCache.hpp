#pragma once

#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "haylen/text/Layout.hpp"
#include "haylen/text/Style.hpp"

namespace haylen::text {

// Keeps the layouts of plain text a font or family made most recently, by text and by the fields of the style that change the layout, so labels and text drawn every frame shape once.
class LayoutCache final {
  public:
    using Builder = std::function<Layout()>;

    // Returns the cached layout of the text in the style, or builds and caches it. A size, maximum width or line spacing that is not finite throws std::invalid_argument.
    [[nodiscard]] std::shared_ptr<const Layout> get(std::string_view text, const Style& style, const Builder& build);

  private:
    // The text and the language are views: a lookup views what it is given, and a cached key views the strings of its entry, which stays where the list built it.
    struct Key {
        std::string_view text;
        float size = 0.0F;
        float maxWidth = 0.0F;
        float lineSpacing = 0.0F;
        Alignment align = Alignment::Start;
        Direction direction = Direction::Auto;
        std::string_view language;
        bool bold = false;
        bool italic = false;

        friend bool operator==(const Key&, const Key&) = default;
    };

    struct KeyHash {
        [[nodiscard]] std::size_t operator()(const Key& key) const noexcept;
    };

    struct Entry {
        std::string text;
        std::string language;
        Key key;
        std::shared_ptr<const Layout> layout;
    };

    static constexpr std::size_t kCapacity = 512;

    std::list<Entry> entries;
    std::unordered_map<Key, std::list<Entry>::iterator, KeyHash> index;
};

} // namespace haylen::text
