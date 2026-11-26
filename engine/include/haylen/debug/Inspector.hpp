#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/debug/Picking.hpp"

namespace haylen::debug {

// Lists and edits the values of the running app for the inspector of the debug tools, by a path of keys from one of its roots, through the provider that the runtime of the scripts of the app gives the debug plugin.
class Inspector final {
  public:
    static constexpr std::size_t kMaxEntries = 200;
    static constexpr std::size_t kMaxValueText = 120;

    struct Entry {
        std::string key;
        std::string type;
        std::string value;
        bool editable = false;
        bool expandable = false;
    };

    // The entries of a listing and the storage that lists them, which a reader keeps so that listing the same values again allocates nothing.
    struct Listing {
        std::vector<Entry> entries;

        // The entries in the order the value holds them, and the places that sort them.
        std::vector<Entry> found;
        std::vector<std::size_t> order;
    };

    // What lists and edits the values of the app, such as the values of its Lua state.
    class Provider {
      public:
        virtual ~Provider() = default;

        // Lists into `listing` the roots for an empty path, and otherwise the keys or properties of the value at the path, sorted by key, at most `kMaxEntries`. Throws `std::invalid_argument` for a path that leads nowhere.
        virtual void list(std::span<const std::string> path, Listing& listing) = 0;

        // Sets the value at the path from text, read as the kind of value it holds now: a number, `true` or `false`, or a string.
        virtual void set(std::span<const std::string> path, std::string_view text) = 0;

        // Makes the thing that the tag of a pick names the root `selected`, and returns `false` when no value of the app answers to the tag, such as a thing that is gone.
        virtual bool select(const Picking::Tag& tag) = 0;
        virtual void clearSelection() = 0;
    };

    Inspector() = delete;

    // Splits a path such as `watched.player.speed` into its keys.
    [[nodiscard]] static std::vector<std::string> splitPath(std::string_view path);

    // Joins keys into a path such as `watched.player.speed`, or adds one key to the end of a path in the storage it holds.
    [[nodiscard]] static std::string joinPath(std::span<const std::string> path);
    static void appendKey(std::string& path, std::string_view key);
};

} // namespace haylen::debug
