#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/debug/Inspector.hpp"

struct lua_State;

namespace haylen::lua::debug {

// Lists and edits the values of the Lua state of the running app for the inspector of the debug tools, by a path of keys from one of its roots: `watched` for the values that `debug.watch` exposes, `scenes` for the scene stack, `autoloads` for the autoloads and `globals` for the global table. Tables list their keys, bound objects list the properties their type declares, and an edit of a number, a boolean or a string goes through the same setter that Lua uses, with its checks.
class ScriptInspector final : public haylen::debug::Inspector::Provider {
  public:
    static constexpr const char* kWatchedKey = "haylen.debug.watched";

    explicit ScriptInspector(lua_State* L) noexcept;

    void list(std::span<const std::string> path, haylen::debug::Inspector::Listing& listing) override;
    void set(std::span<const std::string> path, std::string_view text) override;

    // Exposes a value under `watched` without keeping it alive, or removes it with `nil`.
    static void watch(lua_State* L, std::string_view name, int index);

  private:
    static constexpr std::string_view kRoots[] = {"watched", "scenes", "autoloads", "globals"};

    static void collect(lua_State* L, std::span<const std::string> path, std::vector<haylen::debug::Inspector::Entry>& found, std::size_t& count);

    // Returns the next entry of a listing, which keeps its storage from the listing before.
    [[nodiscard]] static haylen::debug::Inspector::Entry& takeEntry(std::vector<haylen::debug::Inspector::Entry>& found, std::size_t& count);
    static void sort(haylen::debug::Inspector::Listing& listing);
    static void assign(lua_State* L, std::span<const std::string> path, std::string_view text);
    static void pushWatched(lua_State* L);
    static void pushRoot(lua_State* L, std::string_view name);
    static void pushModuleField(lua_State* L, const char* module, const char* field, bool call);
    static void pushPath(lua_State* L, std::span<const std::string> path);
    static void pushKey(lua_State* L, int container, const std::string& key);
    [[nodiscard]] static std::optional<std::int64_t> readIndex(std::string_view key) noexcept;

    // Each description writes into the text of its entry, in the storage that the text holds.
    static void describe(lua_State* L, int index, bool editable, haylen::debug::Inspector::Entry& entry);
    static void describeValue(lua_State* L, int index, std::string& text);
    static void describeKey(lua_State* L, int index, std::string& text);
    static void describeTable(lua_State* L, int index, std::string& text);
    static void appendNumber(lua_State* L, int index, std::string& text);

    // Adds the string at index to the text, in whole characters up to `limit` bytes.
    static void appendText(lua_State* L, int index, std::size_t limit, std::string& text);

    lua_State* state;
};

} // namespace haylen::lua::debug
