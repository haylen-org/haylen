#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "debug/Inspector.hpp"

struct lua_State;

namespace haylen::ui {
class Backend;
}

namespace haylen::debug {

// Draws the inspector with ImGui: a line that says what the window holds, or what the last edit did, and a table of the roots of `Inspector` whose rows open into the values they hold, with a check box for each boolean and a field for each number and string, which edit the value in place. The open rows refresh a few times per second, and nothing runs while the window is closed.
class InspectorWindow final {
  public:
    static constexpr double kRefreshSeconds = 0.25;
    static constexpr float kWidth = 460.0F;
    static constexpr std::string_view kOpenRoot = "scenes";
    static constexpr std::string_view kHint = "The values of the app. A check box or a field changes a value of the app at once, and the app shows the change where it reads that value.";

    // Draws the window and returns `false` once the player closed it.
    bool draw(lua_State* L, ui::Backend& backend, double now);

  private:
    struct Snapshot {
        double time = -1.0;
        std::vector<Inspector::Entry> entries;
        std::string error;
    };

    void drawRows(lua_State* L, std::vector<std::string>& path, double now);
    void drawValue(lua_State* L, const std::vector<std::string>& path, const Inspector::Entry& entry);
    void drawField(lua_State* L, const std::vector<std::string>& path, const Inspector::Entry& entry);
    void edit(lua_State* L, const std::vector<std::string>& path, std::string_view text);
    [[nodiscard]] const Snapshot& snapshotOf(lua_State* L, const std::vector<std::string>& path, double now);
    void reset();

    std::map<std::string, Snapshot> snapshots;
    std::map<std::string, std::string> edits;
    // What the last edit did, or why the setter refused it, which shows in place of the hint until the window closes.
    std::string status;
    bool failed = false;
    std::string activeField;
    bool expired = false;
};

} // namespace haylen::debug
