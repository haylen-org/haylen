#pragma once

#include <cstddef>
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

    // Lists the roots for an empty path, and otherwise the keys or properties of the value at the path, sorted by key, at most `kMaxEntries`.
    [[nodiscard]] static std::vector<Entry> list(lua_State* L, std::span<const std::string> path);

    // Sets the value at the path from text, read as the kind of value it holds now: a number, `true` or `false`, or a string.
    static void set(lua_State* L, std::span<const std::string> path, std::string_view text);

    // Exposes a value under `watched` without keeping it alive, or removes it with `nil`.
    static void watch(lua_State* L, std::string_view name, int index);

  private:
    static constexpr std::string_view kRoots[] = {"watched", "scenes", "autoloads", "globals"};

    static void collect(lua_State* L, std::span<const std::string> path, std::vector<Entry>& entries);
    static void assign(lua_State* L, std::span<const std::string> path, std::string_view text);
    static void pushWatched(lua_State* L);
    static void pushRoot(lua_State* L, std::string_view name);
    static void pushModuleField(lua_State* L, const char* module, const char* field, bool call);
    static void pushPath(lua_State* L, std::span<const std::string> path);
    static void pushChild(lua_State* L, const std::string& key);
    static void pushKey(lua_State* L, int container, const std::string& key);
    [[nodiscard]] static Entry describe(lua_State* L, std::string key, int index, bool editable);
    [[nodiscard]] static std::string describeValue(lua_State* L, int index);
    [[nodiscard]] static std::string describeKey(lua_State* L, int index);
    [[nodiscard]] static std::string joinPath(std::span<const std::string> path);
};

} // namespace haylen::debug
