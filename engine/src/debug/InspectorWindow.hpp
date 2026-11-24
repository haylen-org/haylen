#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/debug/Inspector.hpp"

namespace haylen::ui {
class Backend;
class Context;
} // namespace haylen::ui

namespace haylen::debug {

// Draws the inspector with ImGui: a line that says what the window holds, or what the last edit did, and a table of the roots of `Inspector` whose rows open into the values they hold, with a check box for each boolean and a field for each number and string, which edit the value in place. The open rows refresh a few times per second by the seconds that the frames of the window take, into storage the window keeps with the path of every row, so a frame that shows the same values allocates nothing, and nothing runs while the window is closed. The line reaches screen readers as text of the tree of the UI.
class InspectorWindow final {
  public:
    static constexpr double kRefreshSeconds = 0.25;
    static constexpr float kWidth = 460.0F;
    static constexpr std::string_view kOpenRoot = "scenes";
    static constexpr std::string_view kHint = "The values of the app. A check box or a field changes a value of the app at once, and the app shows the change where it reads that value.";

    // Draws the window, `seconds` after the frame that drew it last, and returns `false` once the player closed it.
    bool draw(Inspector::Provider& provider, ui::Backend& backend, ui::Context& context, double seconds);

  private:
    // The values at one path as the last refresh listed them, with the path of each value.
    struct Snapshot {
        double time = -1.0;
        Inspector::Listing listing;
        std::vector<std::string> paths;
        std::string error;
    };

    // The line that the window shows over its values: what the last edit did, why the setter of the value refused it, or the hint.
    [[nodiscard]] std::string_view getLine() const noexcept {
        return status.empty() ? kHint : std::string_view(status);
    }

    // Each row reads the keys from the root to it in the first `depth` keys of `path`, and `key` is the whole path joined, which names its snapshot and its edit.
    void drawRows(Inspector::Provider& provider, std::size_t depth, const std::string& key, double now);
    void drawValue(Inspector::Provider& provider, std::size_t depth, const std::string& key, const Inspector::Entry& entry);
    void drawField(Inspector::Provider& provider, std::size_t depth, const std::string& key, const Inspector::Entry& entry);
    void edit(Inspector::Provider& provider, std::size_t depth, std::string_view text);
    [[nodiscard]] const Snapshot& snapshotOf(Inspector::Provider& provider, std::size_t depth, const std::string& key, double now);
    void reset();

    double elapsed = 0.0;
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
