#pragma once

#include <map>
#include <string>
#include <vector>

#include "debug/Inspector.hpp"

struct lua_State;

namespace haylen::debug {

// Draws the inspector with ImGui: a tree of the roots of `Inspector` whose open nodes refresh a few times per second, with fields that edit numbers, booleans and strings in place. Nothing runs while the window is closed.
class InspectorWindow final {
  public:
    static constexpr double kRefreshSeconds = 0.25;

    // Draws the window and returns `false` once the player closed it.
    bool draw(lua_State* L, double now);

  private:
    struct Snapshot {
        double time = -1.0;
        std::vector<Inspector::Entry> entries;
        std::string error;
    };

    void drawNode(lua_State* L, std::vector<std::string>& path, double now);
    void drawField(lua_State* L, const std::vector<std::string>& path, const Inspector::Entry& entry);
    [[nodiscard]] const Snapshot& snapshotOf(lua_State* L, const std::vector<std::string>& path, double now);
    [[nodiscard]] static std::string joinPath(const std::vector<std::string>& path);

    std::map<std::string, Snapshot> snapshots;
    std::map<std::string, std::string> edits;
    std::string failure;
    std::string activeField;
    bool expired = false;
};

} // namespace haylen::debug
