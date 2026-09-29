#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <imgui.h>

#include "haylen/math/Rect.hpp"

namespace haylen::ui {

class Component;
class Context;

// Moves items between slot grids and lists, in one document or between documents. The pointer drags them through ImGui drag and drop, and the keyboard, gamepads and remotes carry them: accept picks the focused item up, the focus moves, and accept drops it. Components only report the move, so the app changes its own data.
class DragAndDrop final {
  public:
    // What a component learns about the item it drew last: whether the player picked it up or drags it, whether something is carried or dragged over it, and the node and entry dropped on it.
    struct Result {
        bool picked = false;
        bool dragged = false;
        bool hovering = false;
        std::optional<std::pair<std::string, std::string>> dropped;
    };

    // Runs after the item of an entry is drawn: offers it to the pointer as a drag source, takes pointer drops on it, and picks up or drops carried items when the player activates it with accept.
    [[nodiscard]] static Result handle(Context& context, const Component& component, std::string_view entry, std::string_view image, const math::Rect& bounds);

    // Draws the picture of a carried item over the focused entry, so the player sees what moves.
    static void drawCarried(Context& context, ImGuiID item, const math::Rect& bounds);

  private:
    static constexpr const char* kPayload = "haylen.item";
};

} // namespace haylen::ui
