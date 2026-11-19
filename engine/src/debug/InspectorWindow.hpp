#pragma once

#include <cstddef>
#include <functional>
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

// Draws the inspector with ImGui: a line that says what the window holds, or what the last edit did, and a table of the roots of `Inspector` whose rows open into the values they hold, with a check box for each boolean and a field for each number and string, which edit the value in place. The open rows refresh a few times per second by the clock of the window, into storage the window keeps with the path of every row, so a frame that shows the same values allocates nothing, and nothing runs while the window is closed.
class InspectorWindow final {
  public:
    // Answers the seconds that the refreshes count, the steady clock of the system unless a caller, such as a test, gives another.
    using Clock = std::function<double()>;

    static constexpr double kRefreshSeconds = 0.25;
    static constexpr float kWidth = 460.0F;
    static constexpr std::string_view kOpenRoot = "scenes";
    static constexpr std::string_view kHint = "The values of the app. A check box or a field changes a value of the app at once, and the app shows the change where it reads that value.";

    explicit InspectorWindow(Clock source = {});

    // Draws the window and returns `false` once the player closed it.
    bool draw(lua_State* L, ui::Backend& backend);

  private:
    // The values at one path as the last refresh listed them, with the path of each value.
    struct Snapshot {
        double time = -1.0;
        Inspector::Listing listing;
        std::vector<std::string> paths;
        std::string error;
    };

    [[nodiscard]] static double getSteadySeconds();

    // Each row reads the keys from the root to it in the first `depth` keys of `path`, and `key` is the whole path joined, which names its snapshot and its edit.
    void drawRows(lua_State* L, std::size_t depth, const std::string& key, double now);
    void drawValue(lua_State* L, std::size_t depth, const std::string& key, const Inspector::Entry& entry);
    void drawField(lua_State* L, std::size_t depth, const std::string& key, const Inspector::Entry& entry);
    void edit(lua_State* L, std::size_t depth, std::string_view text);
    [[nodiscard]] const Snapshot& snapshotOf(lua_State* L, std::size_t depth, const std::string& key, double now);
    void reset();

    Clock clock;
    std::map<std::string, Snapshot, std::less<>> snapshots;
    std::map<std::string, std::string, std::less<>> edits;
    std::vector<std::string> path;
    std::string rootPath;

    // What the last edit did, or why the setter refused it, which shows in place of the hint until the window closes.
    std::string status;
    bool failed = false;
    std::string activeField;
    bool expired = false;
};

} // namespace haylen::debug
