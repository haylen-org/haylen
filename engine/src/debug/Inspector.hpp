#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct lua_State;

namespace haylen::debug {

// Lists and edits the values of the running app for the inspector of the debug tools, by a path of keys from one of its roots: `watched` for the values that `debug.watch` exposes, `scenes` for the scene stack, `autoloads` for the autoloads and `globals` for the global table. Tables list their keys, bound objects list the properties their type declares, and an edit of a number, a boolean or a string goes through the same setter that Lua uses, with its checks.
class Inspector final {
  public:
    static constexpr std::size_t kMaxEntries = 200;
    static constexpr std::size_t kMaxValueText = 120;
    static constexpr const char* kWatchedKey = "haylen.debug.watched";

    struct Entry {
        std::string key;
        std::string type;
        std::string value;
        bool editable = false;
        bool expandable = false;
    };

    // Splits a path such as `watched.player.speed` into its keys.
    [[nodiscard]] static std::vector<std::string> splitPath(std::string_view path);

    // The entries of a listing and the storage that lists them, which a reader keeps so that listing the same values again allocates nothing.
    struct Listing {
        std::vector<Entry> entries;

        // The entries in the order the value holds them, and the places that sort them.
        std::vector<Entry> found;
        std::vector<std::size_t> order;
    };

    // Lists into `listing` the roots for an empty path, and otherwise the keys or properties of the value at the path, sorted by key, at most `kMaxEntries`.
    static void list(lua_State* L, std::span<const std::string> path, Listing& listing);

    // Sets the value at the path from text, read as the kind of value it holds now: a number, `true` or `false`, or a string.
    static void set(lua_State* L, std::span<const std::string> path, std::string_view text);

    // Exposes a value under `watched` without keeping it alive, or removes it with `nil`.
    static void watch(lua_State* L, std::string_view name, int index);

    // Joins keys into a path such as `watched.player.speed`, or adds one key to the end of a path in the storage it holds.
    [[nodiscard]] static std::string joinPath(std::span<const std::string> path);
    static void appendKey(std::string& path, std::string_view key);

  private:
    static constexpr std::string_view kRoots[] = {"watched", "scenes", "autoloads", "globals"};

    static void collect(lua_State* L, std::span<const std::string> path, std::vector<Entry>& found, std::size_t& count);

    // Returns the next entry of a listing, which keeps its storage from the listing before.
    [[nodiscard]] static Entry& takeEntry(std::vector<Entry>& found, std::size_t& count);
    static void sort(Listing& listing);
    static void assign(lua_State* L, std::span<const std::string> path, std::string_view text);
    static void pushWatched(lua_State* L);
    static void pushRoot(lua_State* L, std::string_view name);
    static void pushModuleField(lua_State* L, const char* module, const char* field, bool call);
    static void pushPath(lua_State* L, std::span<const std::string> path);
    static void pushKey(lua_State* L, int container, const std::string& key);
    [[nodiscard]] static std::optional<std::int64_t> readIndex(std::string_view key) noexcept;

    // Each description writes into the text of its entry, in the storage that the text holds.
    static void describe(lua_State* L, int index, bool editable, Entry& entry);
    static void describeValue(lua_State* L, int index, std::string& text);
    static void describeKey(lua_State* L, int index, std::string& text);
    static void describeTable(lua_State* L, int index, std::string& text);
    static void appendNumber(lua_State* L, int index, std::string& text);

    // Adds the string at index to the text, in whole characters up to `limit` bytes.
    static void appendText(lua_State* L, int index, std::size_t limit, std::string& text);
};

} // namespace haylen::debug
