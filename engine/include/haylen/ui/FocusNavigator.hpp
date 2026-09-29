#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>

#include "haylen/input/InputDevice.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/ui/FocusDirection.hpp"
#include "haylen/ui/FocusWrap.hpp"

struct ImGuiWindow;

namespace haylen::ui {

class Component;
class Document;
class NavigationInput;

// Moves the keyboard, gamepad and remote focus between the controls of mounted documents. Controls register as targets while they draw, and at the start of the next frame the navigation actions move the focus among them, press the focused control or go back, following explicit neighbors, focus scopes and wrap options. The focus stays inside the document that holds it and inside a popup, such as a dialog, that holds it.
class FocusNavigator final {
  public:
    // An item a player carries with the keyboard or a gamepad to drop on another item, the way the pointer drags it.
    struct Carry {
        std::string source;
        std::string item;
        std::string image;
    };

    static constexpr std::size_t kHistoryLimit = 32;

    // Reads the navigation actions, which the backend hands ImGui as gamepad keys, and moves, presses or cancels among the targets of the last frame. It runs after ImGui starts the frame and before documents draw.
    void update(const NavigationInput& navigation, input::InputDevice lastDevice, bool hasPointerDevice);

    void beginDraw();
    void beginDocument(const Document& document);
    void enter(const Component& component, ImGuiID node, const math::Rect& bounds);
    void leave();

    // Registers an item of the component being drawn as a place the focus can go, unless it or a node around it cannot take the focus or it is disabled.
    void addTarget(ImGuiID item, const math::Rect& bounds);
    void endDraw();

    // Keeps the controls drawn meanwhile out of navigation, such as those of a carousel page sliding out.
    void suspendTargets(bool value) noexcept {
        suspended = value;
    }

    // Gives the focus to an item of the window being drawn, as a script or a component asks. The ring shows when the player already navigates, last used a gamepad or has no pointer device.
    void focus(ImGuiID item, const math::Rect& bounds);
    void clear();

    // Whether the component being drawn and every node around it can take the focus.
    [[nodiscard]] bool isFocusable() const noexcept;

    // Whether the focus is on a control of the document and ImGui window being drawn, which autofocus leaves alone.
    [[nodiscard]] bool hasFocusHere() const;

    // Returns where the focused control of the ImGui window being drawn lies when it lies inside the area, such as the child of a context menu.
    [[nodiscard]] std::optional<math::Rect> findFocusIn(const math::Rect& area) const;

    [[nodiscard]] bool isRingVisible() const noexcept {
        return ringVisible;
    }
    [[nodiscard]] bool isRingShown(ImGuiID item) const;

    // Whether the player pressed ui_cancel this frame for a window that handles it itself, such as a dialog: the topmost popup, or the window with the focus while no popup is open.
    [[nodiscard]] bool isCancelPressedIn(const ImGuiWindow* window) const noexcept;

    // Whether the player pressed ui_menu this frame, which opens the context menu around the focus.
    [[nodiscard]] bool isMenuPressed() const noexcept {
        return menuPressed;
    }

    // Cancel drops a carried item instead of reaching a document.
    void carry(Carry value);
    void dropCarried() noexcept {
        carried.reset();
    }
    [[nodiscard]] const std::optional<Carry>& getCarried() const noexcept {
        return carried;
    }

    // Takes the direction the focused component uses itself this frame, such as left and right on a slider.
    [[nodiscard]] std::optional<FocusDirection> takeDirection(ImGuiID node);

    // Takes the events waiting for a node: focus, blur and cancel.
    [[nodiscard]] std::vector<std::string_view> takeNotices(ImGuiID node);

    // The document and the node id that hold the focus, or nothing. The document is only compared, never read, since it may have been unmounted.
    [[nodiscard]] const Document* getFocusedDocument() const noexcept;
    [[nodiscard]] std::string_view getFocusedName() const noexcept;

  private:
    struct Node {
        ImGuiID id = 0;
        const Document* document = nullptr;
        std::string name;
        std::array<std::string, 4> neighbors;
        std::uint8_t usedDirections = 0;
        std::optional<std::size_t> scope;
    };

    struct Scope {
        ImGuiID id = 0;
        math::Rect bounds;
        bool trap = false;
        FocusWrap wrap = FocusWrap::None;
        std::optional<std::size_t> parent;
    };

    struct Target {
        ImGuiID id = 0;
        ImGuiWindow* window = nullptr;
        math::Rect bounds;
        std::size_t node = 0;
        bool inputable = false;
    };

    struct DocumentEntry {
        const Document* document = nullptr;
        ImGuiWindow* window = nullptr;
        ImGuiID root = 0;
    };

    // Everything one frame of drawing registered.
    struct Frame {
        std::vector<Node> nodes;
        std::vector<Scope> scopes;
        std::vector<Target> targets;
        std::vector<DocumentEntry> documents;
    };

    // A component being drawn, with the node and the scope its targets belong to.
    struct Level {
        const Component* component = nullptr;
        ImGuiID id = 0;
        bool focusable = true;
        std::optional<std::size_t> node;
        std::optional<std::size_t> scope;
    };

    [[nodiscard]] static bool isManaged(const ImGuiWindow* window) noexcept;
    [[nodiscard]] static std::uint8_t toBit(FocusDirection direction) noexcept;
    [[nodiscard]] static bool wrapsAlong(FocusWrap wrap, FocusDirection direction) noexcept;
    static void setFocus(ImGuiID item, ImGuiWindow* window, const math::Rect& bounds);

    [[nodiscard]] const Target* find(const Frame& frame, ImGuiID item) const noexcept;
    [[nodiscard]] const Target* findNamed(const Document* document, std::string_view name) const noexcept;
    [[nodiscard]] std::optional<std::size_t> findTrap(std::optional<std::size_t> scope) const noexcept;
    [[nodiscard]] std::optional<std::size_t> findWrap(std::optional<std::size_t> scope, std::optional<std::size_t> trap, FocusDirection direction) const noexcept;
    [[nodiscard]] bool isInside(const Target& target, std::optional<std::size_t> scope) const noexcept;
    [[nodiscard]] bool isPeer(const Target& source, const Target& target) const noexcept;
    [[nodiscard]] const Target* search(const Target& source, const math::Rect& from, std::optional<std::size_t> scope, FocusDirection direction) const;
    [[nodiscard]] const Target* searchAround(const Target& source, FocusDirection direction) const;
    [[nodiscard]] ImGuiID getTrapId(const Frame& frame, const Target& target) const noexcept;

    void move(const Target& source, FocusDirection direction);
    void tab(const Target& source, bool backward);
    void apply(const Target& target);
    void activate(const Target& target);
    void cancel(const Target* source);
    void remember(const Frame& frame, const Target& next);
    void restore();
    void focusPopup();
    void notify(ImGuiID node, std::string_view name);

    Frame drawn;
    Frame building;
    std::vector<Level> levels;
    const Document* currentDocument = nullptr;
    std::vector<ImGuiID> history;
    std::vector<std::pair<ImGuiID, std::string_view>> notices;
    std::optional<std::pair<ImGuiID, FocusDirection>> pendingDirection;
    std::optional<Carry> carried;
    ImGuiID focusedItem = 0;
    ImGuiID focusedNode = 0;
    const Document* focusedDocument = nullptr;
    std::string focusedName;
    ImGuiWindow* focusedRoot = nullptr;
    math::Rect focusedBounds;
    int popupsAtEnd = 0;
    ImGuiWindow* popupAtEnd = nullptr;
    ImGuiID activeAtEnd = 0;
    input::InputDevice lastInput = input::InputDevice::KeyboardMouse;
    bool pointerAvailable = true;
    bool ringVisible = false;
    bool cancelPressed = false;
    bool menuPressed = false;
    bool suspended = false;
};

} // namespace haylen::ui
