#include "haylen/ui/FocusNavigator.hpp"

#include <algorithm>

#include <imgui_internal.h>

#include "haylen/ui/Component.hpp"
#include "haylen/ui/NavigationInput.hpp"
#include "ui/FocusSearch.hpp"
#include "ui/ImGuiConverter.hpp"

namespace haylen::ui {

bool FocusNavigator::isManaged(const ImGuiWindow* window) noexcept {
    return window != nullptr && (window->Flags & ImGuiWindowFlags_NoNavInputs) != 0;
}

std::uint8_t FocusNavigator::toBit(FocusDirection direction) noexcept {
    return static_cast<std::uint8_t>(1U << static_cast<unsigned>(direction));
}

bool FocusNavigator::wrapsAlong(FocusWrap wrap, FocusDirection direction) noexcept {
    const bool horizontal = direction == FocusDirection::Left || direction == FocusDirection::Right;
    return wrap == FocusWrap::Both || wrap == (horizontal ? FocusWrap::Horizontal : FocusWrap::Vertical);
}

// Focus that moves into a draggable window brings it to the front, like a click would, and a scrolled container scrolls the item into view.
void FocusNavigator::setFocus(ImGuiID item, ImGuiWindow* window, const math::Rect& bounds) {
    const ImGuiWindow* current = GImGui->NavWindow;
    if ((window->RootWindow->Flags & ImGuiWindowFlags_Popup) == 0 && (current == nullptr || current->RootWindow != window->RootWindow)) {
        ImGui::FocusWindow(window);
    }
    ImGui::SetFocusID(item, window);
    if ((window->Flags & ImGuiWindowFlags_ChildWindow) != 0) {
        ImGui::ScrollToRectEx(window, ImGuiConverter::toImRect(bounds), ImGuiScrollFlags_KeepVisibleEdgeX | ImGuiScrollFlags_KeepVisibleEdgeY | ImGuiScrollFlags_NoScrollParent);
    }
}

void FocusNavigator::update(const NavigationInput& navigation, input::InputDevice lastDevice, bool hasPointerDevice) {
    ImGuiContext& g = *GImGui;
    lastInput = lastDevice;
    pointerAvailable = hasPointerDevice;
    pendingDirection.reset();

    // A player without a pointer only navigates, so the ring always shows, and a pointer press hides it until the player navigates again.
    if (!hasPointerDevice) {
        ringVisible = true;
    } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
        ringVisible = false;
    }

    const Target* current = find(drawn, g.NavId);
    cancelPressed = false;
    menuPressed = false;
    if (g.NavWindow != nullptr && !isManaged(g.NavWindow)) {
        return;
    }
    menuPressed = activeAtEnd == 0 && g.ActiveId == 0 && navigation.isPressed(NavigationInput::Action::Menu);

    // Cancel belongs to a control being edited when the frame starts with one, which ImGui already let go of.
    cancelPressed = activeAtEnd == 0 && g.ActiveId == 0 && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, ImGuiInputFlags_None, ImGuiKeyOwner_NoOwner);
    const bool accept = ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, ImGuiInputFlags_None, ImGuiKeyOwner_NoOwner);
    if (current != nullptr && g.ActiveId == current->id && ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown, ImGuiKeyOwner_NoOwner)) {
        g.NavActivateDownId = current->id;
    }

    // Tab walks the controls in the order they draw, also out of a text field being edited, the way ImGui tabs through a window.
    const bool tabbing = !g.IO.KeyCtrl && !g.IO.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Tab, ImGuiInputFlags_Repeat, ImGuiKeyOwner_NoOwner);
    if (tabbing && current != nullptr && (g.ActiveId == 0 || g.ActiveId == current->id)) {
        tab(*current, g.IO.KeyShift);
        return;
    }
    if (g.ActiveId != 0) {
        return;
    }

    if (cancelPressed && carried) {
        carried.reset();
        cancelPressed = false;
    }

    // ImGui closes the popups the UI opens, and dialogs and windows take cancel themselves, so documents only hear it while no popup was open.
    if (cancelPressed && popupsAtEnd == 0 && g.OpenPopupStack.Size == 0) {
        cancel(current);
    }
    if (current == nullptr) {
        return;
    }
    if (accept) {
        ringVisible = true;
        activate(*current);
        return;
    }

    constexpr std::array<std::pair<ImGuiKey, FocusDirection>, 4> kKeys{{
        {ImGuiKey_GamepadDpadLeft, FocusDirection::Left},
        {ImGuiKey_GamepadDpadRight, FocusDirection::Right},
        {ImGuiKey_GamepadDpadUp, FocusDirection::Up},
        {ImGuiKey_GamepadDpadDown, FocusDirection::Down},
    }};
    const ImGuiInputFlags repeat = static_cast<ImGuiInputFlags>(ImGuiInputFlags_Repeat) | static_cast<ImGuiInputFlags>(ImGuiInputFlags_RepeatRateNavMove);
    for (const auto& [key, direction] : kKeys) {
        if (ImGui::IsKeyPressed(key, repeat, ImGuiKeyOwner_NoOwner)) {
            move(*current, direction);
            return;
        }
    }
}

void FocusNavigator::beginDraw() {
    building.nodes.clear();
    building.scopes.clear();
    building.targets.clear();
    building.documents.clear();
    levels.clear();
    cancelWindows.clear();
    currentDocument = nullptr;
}

void FocusNavigator::beginDocument(const Document& document) {
    currentDocument = &document;
    building.documents.push_back({.document = &document, .window = ImGui::GetCurrentWindow()->RootWindow, .root = 0});
}

void FocusNavigator::enter(const Component& component, ImGuiID node, const math::Rect& bounds) {
    const Component::CommonProperties& common = component.getCommon();
    Level level{.component = &component, .id = node};
    if (!levels.empty()) {
        level.focusable = levels.back().focusable;
        level.scope = levels.back().scope;
    }
    level.focusable = level.focusable && common.focusable;
    if (common.focusScope || common.focusWrap != FocusWrap::None) {
        building.scopes.push_back({.id = node, .bounds = bounds, .trap = common.focusScope, .wrap = common.focusWrap, .parent = level.scope});
        level.scope = building.scopes.size() - 1;
    }
    if (!building.documents.empty() && building.documents.back().root == 0) {
        building.documents.back().root = node;
    }
    levels.push_back(level);
}

void FocusNavigator::leave() {
    levels.pop_back();
}

void FocusNavigator::addTarget(ImGuiID item, const math::Rect& bounds) {
    if (suspended || levels.empty() || !levels.back().focusable || (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0) {
        return;
    }

    // A component becomes a node the first time it registers a target in a frame, so components without targets cost nothing.
    Level& level = levels.back();
    if (!level.node) {
        Node node{.id = level.id, .document = currentDocument, .name = level.component->getId(), .neighbors = level.component->getCommon().focusNeighbors, .usedDirections = 0, .scope = level.scope};
        for (const FocusDirection direction : {FocusDirection::Left, FocusDirection::Right, FocusDirection::Up, FocusDirection::Down}) {
            if (level.component->usesFocusDirection(direction)) {
                node.usedDirections |= toBit(direction);
            }
        }
        building.nodes.push_back(std::move(node));
        level.node = building.nodes.size() - 1;
    }
    const bool inputable = GImGui->LastItemData.ID == item && (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Inputable) != 0;
    building.targets.push_back({.id = item, .window = ImGui::GetCurrentWindow(), .bounds = bounds, .node = *level.node, .inputable = inputable});
}

void FocusNavigator::endDraw() {
    ImGuiContext& g = *GImGui;
    std::swap(drawn, building);
    levels.clear();
    notices.clear();
    popupsAtEnd = g.OpenPopupStack.Size;
    popupAtEnd = popupsAtEnd > 0 ? g.OpenPopupStack.back().Window : nullptr;
    activeAtEnd = g.ActiveId;

    if (find(drawn, g.NavId) == nullptr && focusedItem != 0 && (g.NavId == focusedItem || g.NavId == 0)) {
        restore();
    }
    focusPopup();

    const Target* focused = find(drawn, g.NavId);
    const Node* node = focused != nullptr ? &drawn.nodes[focused->node] : nullptr;
    const ImGuiID owner = node != nullptr ? node->id : 0;
    if (owner != focusedNode) {
        if (focusedNode != 0) {
            notify(focusedNode, "blur");
        }
        if (owner != 0) {
            notify(owner, "focus");
        }
        focusedNode = owner;
    }
    focusedItem = focused != nullptr ? focused->id : 0;
    focusedDocument = node != nullptr ? node->document : nullptr;
    focusedName = node != nullptr ? node->name : std::string();
    focusedRoot = focused != nullptr ? focused->window->RootWindow : nullptr;
    focusedBounds = focused != nullptr ? focused->bounds : math::Rect{};

    editing = g.ActiveId != 0 && (g.ActiveIdWindow == nullptr || g.ActiveId != g.ActiveIdWindow->MoveId);
    const ImGuiWindow* answering = popupAtEnd != nullptr ? popupAtEnd : focusedRoot;
    cancelAnswered = editing || popupsAtEnd > 0 || carried.has_value() || std::ranges::find(cancelWindows, answering) != cancelWindows.end();
    acceptAnswered = editing || focusedItem != 0;
}

void FocusNavigator::focus(ImGuiID item, const math::Rect& bounds) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ringVisible = ringVisible || lastInput == input::InputDevice::Gamepad || !pointerAvailable;
    if (const Target* target = find(building, item)) {
        remember(building, *target);
    }
    setFocus(item, window, bounds);
    focusedDocument = currentDocument;
    focusedRoot = window->RootWindow;
    focusedBounds = bounds;
}

void FocusNavigator::clear() {
    ImGuiContext& g = *GImGui;
    if (find(drawn, g.NavId) != nullptr || find(building, g.NavId) != nullptr) {
        g.NavId = 0;
    }
    history.clear();
}

void FocusNavigator::carry(Carry value) {
    carried = std::move(value);
}

bool FocusNavigator::isFocusable() const noexcept {
    return levels.empty() || levels.back().focusable;
}

bool FocusNavigator::hasFocusHere() const {
    const ImGuiContext& g = *GImGui;
    return g.NavId != 0 && focusedDocument == currentDocument && focusedRoot == ImGui::GetCurrentWindow()->RootWindow;
}

bool FocusNavigator::answerCancel(const ImGuiWindow* window) {
    if (window == nullptr) {
        return false;
    }
    cancelWindows.push_back(window->RootWindow);
    if (!cancelPressed) {
        return false;
    }
    return popupAtEnd != nullptr ? popupAtEnd == window : focusedRoot == window->RootWindow;
}

std::optional<math::Rect> FocusNavigator::findFocusIn(const math::Rect& area) const {
    if (focusedItem == 0 || focusedRoot != ImGui::GetCurrentWindow()->RootWindow || !area.contains(focusedBounds.getCenter())) {
        return std::nullopt;
    }
    return focusedBounds;
}

bool FocusNavigator::isRingShown(ImGuiID item) const {
    return ringVisible && item == GImGui->NavId;
}

std::optional<FocusDirection> FocusNavigator::takeDirection(ImGuiID node) {
    if (!pendingDirection || pendingDirection->first != node) {
        return std::nullopt;
    }
    return std::exchange(pendingDirection, std::nullopt)->second;
}

std::vector<std::string_view> FocusNavigator::takeNotices(ImGuiID node) {
    std::vector<std::string_view> names;
    if (notices.empty()) {
        return names;
    }
    for (const auto& [target, name] : notices) {
        if (target == node) {
            names.push_back(name);
        }
    }
    std::erase_if(notices, [node](const auto& notice) { return notice.first == node; });
    return names;
}

const Document* FocusNavigator::getFocusedDocument() const noexcept {
    return focusedItem != 0 ? focusedDocument : nullptr;
}

std::string_view FocusNavigator::getFocusedName() const noexcept {
    return focusedName;
}

const FocusNavigator::Target* FocusNavigator::find(const Frame& frame, ImGuiID item) const noexcept {
    if (item == 0) {
        return nullptr;
    }
    const auto found = std::ranges::find(frame.targets, item, &Target::id);
    return found != frame.targets.end() ? &*found : nullptr;
}

const FocusNavigator::Target* FocusNavigator::findNamed(const Document* document, std::string_view name) const noexcept {
    const auto found = std::ranges::find_if(drawn.targets, [&](const Target& target) { return drawn.nodes[target.node].document == document && drawn.nodes[target.node].name == name; });
    return found != drawn.targets.end() ? &*found : nullptr;
}

std::optional<std::size_t> FocusNavigator::findTrap(std::optional<std::size_t> scope) const noexcept {
    while (scope && !drawn.scopes[*scope].trap) {
        scope = drawn.scopes[*scope].parent;
    }
    return scope;
}

std::optional<std::size_t> FocusNavigator::findWrap(std::optional<std::size_t> scope, std::optional<std::size_t> trap, FocusDirection direction) const noexcept {
    for (; scope; scope = drawn.scopes[*scope].parent) {
        if (wrapsAlong(drawn.scopes[*scope].wrap, direction)) {
            return scope;
        }
        if (scope == trap) {
            break;
        }
    }
    return std::nullopt;
}

bool FocusNavigator::isInside(const Target& target, std::optional<std::size_t> scope) const noexcept {
    if (!scope) {
        return true;
    }
    for (std::optional<std::size_t> current = drawn.nodes[target.node].scope; current; current = drawn.scopes[*current].parent) {
        if (current == scope) {
            return true;
        }
    }
    return false;
}

// Peers share the document and the innermost focus scope, so the focus never enters a scope it is not in, and a popup keeps the focus inside it.
bool FocusNavigator::isPeer(const Target& source, const Target& target) const noexcept {
    const Node& origin = drawn.nodes[source.node];
    const Node& node = drawn.nodes[target.node];
    const bool popup = ((source.window->RootWindow->Flags | target.window->RootWindow->Flags) & ImGuiWindowFlags_Popup) != 0;
    return node.document == origin.document && (target.window->RootWindow == source.window->RootWindow || !popup) && findTrap(node.scope) == findTrap(origin.scope);
}

const FocusNavigator::Target* FocusNavigator::search(const Target& source, const math::Rect& from, std::optional<std::size_t> scope, FocusDirection direction) const {
    std::vector<math::Rect> bounds;
    std::vector<const Target*> candidates;
    for (const Target& target : drawn.targets) {
        if (target.id != source.id && isPeer(source, target) && isInside(target, scope)) {
            bounds.push_back(target.bounds);
            candidates.push_back(&target);
        }
    }
    const std::optional<std::size_t> found = FocusSearch::find(from, bounds, direction);
    return found ? candidates[*found] : nullptr;
}

// A node that wraps along the direction keeps the move inside it and starts again from its other side when nothing lies ahead.
const FocusNavigator::Target* FocusNavigator::searchAround(const Target& source, FocusDirection direction) const {
    const Node& node = drawn.nodes[source.node];
    const std::optional<std::size_t> trap = findTrap(node.scope);
    const std::optional<std::size_t> wrap = findWrap(node.scope, trap, direction);
    if (!wrap) {
        return search(source, source.bounds, trap, direction);
    }
    if (const Target* ahead = search(source, source.bounds, wrap, direction)) {
        return ahead;
    }
    return search(source, FocusSearch::wrap(source.bounds, drawn.scopes[*wrap].bounds, direction), wrap, direction);
}

ImGuiID FocusNavigator::getTrapId(const Frame& frame, const Target& target) const noexcept {
    for (std::optional<std::size_t> scope = frame.nodes[target.node].scope; scope; scope = frame.scopes[*scope].parent) {
        if (frame.scopes[*scope].trap) {
            return frame.scopes[*scope].id;
        }
    }
    return 0;
}

void FocusNavigator::move(const Target& source, FocusDirection direction) {
    // The first move of a player who could not see the ring only shows where the focus is.
    if (!ringVisible) {
        ringVisible = true;
        return;
    }

    const Node& node = drawn.nodes[source.node];
    if ((node.usedDirections & toBit(direction)) != 0) {
        pendingDirection = {node.id, direction};
        return;
    }

    // A neighbor named explicitly wins over the search, and a node that names itself keeps the focus.
    const std::string& neighbor = node.neighbors[static_cast<std::size_t>(direction)];
    if (!neighbor.empty() && neighbor == node.name) {
        return;
    }
    if (!neighbor.empty()) {
        if (const Target* target = findNamed(node.document, neighbor)) {
            apply(*target);
            return;
        }
    }
    if (const Target* target = searchAround(source, direction)) {
        apply(*target);
    }
}

// A text field that tab reaches starts editing, so typing moves on from field to field.
void FocusNavigator::tab(const Target& source, bool backward) {
    std::vector<const Target*> peers;
    for (const Target& target : drawn.targets) {
        if (isPeer(source, target)) {
            peers.push_back(&target);
        }
    }
    const auto position = static_cast<std::size_t>(std::ranges::find(peers, &source) - peers.begin());
    const Target& next = *peers[(position + (backward ? peers.size() - 1 : 1)) % peers.size()];
    if (GImGui->ActiveId != 0) {
        ImGui::ClearActiveID();
    }
    ringVisible = true;
    apply(next);
    if (next.inputable) {
        activate(next);
    }
}

void FocusNavigator::apply(const Target& target) {
    remember(drawn, target);
    setFocus(target.id, target.window, target.bounds);
    focusedDocument = drawn.nodes[target.node].document;
    focusedRoot = target.window->RootWindow;
    focusedBounds = target.bounds;
}

// Activation happens the way ImGui activates an item from the keyboard, preferring text input, so a text field starts editing.
void FocusNavigator::activate(const Target& target) {
    ImGuiContext& g = *GImGui;
    g.NavInputSource = ImGuiInputSource_Gamepad;
    g.NavActivateId = target.id;
    g.NavActivateDownId = target.id;
    g.NavActivatePressedId = target.id;
    g.NavActivateFlags = ImGuiActivateFlags_PreferInput;
}

// Cancel goes to the innermost focus scope around the focus, or to the root of the document that holds it, or of the topmost document when nothing has the focus. Focus in a window of its own, such as a draggable window, leaves cancel to that window.
void FocusNavigator::cancel(const Target* source) {
    const Node* node = source != nullptr ? &drawn.nodes[source->node] : nullptr;
    if (node != nullptr) {
        if (const std::optional<std::size_t> trap = findTrap(node->scope)) {
            notify(drawn.scopes[*trap].id, "cancel");
            return;
        }
    }
    if (drawn.documents.empty()) {
        return;
    }
    const Document* document = node != nullptr ? node->document : drawn.documents.back().document;
    const auto entry = std::ranges::find(drawn.documents, document, &DocumentEntry::document);
    if (entry != drawn.documents.end() && entry->root != 0 && (source == nullptr || source->window->RootWindow == entry->window)) {
        notify(entry->root, "cancel");
    }
}

// A move into another document, ImGui window or focus scope remembers where the focus was, so it goes back there when the scope closes.
void FocusNavigator::remember(const Frame& frame, const Target& next) {
    const Target* current = find(drawn, GImGui->NavId);
    if (current == nullptr || current->id == next.id) {
        return;
    }
    const bool elsewhere = drawn.nodes[current->node].document != frame.nodes[next.node].document || current->window->RootWindow != next.window->RootWindow || getTrapId(drawn, *current) != getTrapId(frame, next);
    if (!elsewhere) {
        return;
    }
    std::erase(history, current->id);
    history.push_back(current->id);
    if (history.size() > kHistoryLimit) {
        history.erase(history.begin());
    }
}

// Lost focus goes back to where it was before it entered the scope that closed, for players who navigate. A pointer player simply loses it, so a key that also plays the game never presses a control nobody chose.
void FocusNavigator::restore() {
    ImGuiContext& g = *GImGui;
    while (ringVisible && !history.empty()) {
        const ImGuiID item = history.back();
        history.pop_back();
        if (const Target* target = find(drawn, item)) {
            setFocus(target->id, target->window, target->bounds);
            return;
        }
    }
    g.NavId = 0;
}

// A popup the UI draws takes the focus when it opens, on its first control.
void FocusNavigator::focusPopup() {
    ImGuiContext& g = *GImGui;
    if (g.OpenPopupStack.Size == 0) {
        return;
    }
    ImGuiWindow* popup = g.OpenPopupStack.back().Window;
    const Target* focused = find(drawn, g.NavId);
    if (!isManaged(popup) || (focused != nullptr && focused->window->RootWindow == popup)) {
        return;
    }
    const auto first = std::ranges::find_if(drawn.targets, [popup](const Target& target) { return target.window->RootWindow == popup; });
    if (first != drawn.targets.end()) {
        remember(drawn, *first);
        ImGui::SetFocusID(first->id, first->window);
    }
}

void FocusNavigator::notify(ImGuiID node, std::string_view name) {
    notices.emplace_back(node, name);
}

} // namespace haylen::ui
