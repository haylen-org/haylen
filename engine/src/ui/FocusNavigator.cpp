#include "haylen/ui/FocusNavigator.hpp"

#include <algorithm>

#include <imgui_internal.h>

#include "haylen/ui/Component.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/NavigationInput.hpp"
#include "ui/FocusSearch.hpp"
#include "ui/ImGuiConverter.hpp"

namespace haylen::ui {

const ImGuiInputFlags FocusNavigator::kRepeat = static_cast<ImGuiInputFlags>(ImGuiInputFlags_Repeat) | static_cast<ImGuiInputFlags>(ImGuiInputFlags_RepeatRateNavMove);

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
    pendingPaging.reset();
    reveal = 0;

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

    // Tab walks the controls and the play areas in the order they draw, also out of a text field being edited, the way ImGui tabs through a window.
    const bool tabbing = !g.IO.KeyCtrl && !g.IO.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Tab, ImGuiInputFlags_Repeat, ImGuiKeyOwner_NoOwner);
    if (tabbing && current != nullptr && (g.ActiveId == 0 || g.ActiveId == current->id)) {
        tab(*current, g.IO.KeyShift);
        return;
    }
    if (g.ActiveId != 0) {
        return;
    }
    if (navigation.isPressed(NavigationInput::Action::Focus)) {
        switchFocus(current);
        return;
    }

    if (cancelPressed && carried) {
        carried.reset();
        cancelPressed = false;
    }

    // ImGui closes the popups the UI opens, and dialogs and windows take cancel themselves, so GUIs only hear it while no popup was open.
    if (cancelPressed && popupsAtEnd == 0 && g.OpenPopupStack.Size == 0) {
        cancel(current);
    }
    // The directions, accept and menu of a play area belong to the game, which reads them from the action map.
    if (current == nullptr || current->play || takeRevealing(*current, accept)) {
        return;
    }
    if (accept) {
        ringVisible = true;
        activate(*current);
        return;
    }
    if (takePageAction(navigation, *current)) {
        return;
    }
    for (const auto& [key, direction] : kDirectionKeys) {
        if (ImGui::IsKeyPressed(key, kRepeat, ImGuiKeyOwner_NoOwner)) {
            move(*current, direction);
            return;
        }
    }
}

// A direction or accept while the focused control lies outside the clip of its window, such as an item a collection scrolled away, brings it back into view instead of moving or pressing.
bool FocusNavigator::takeRevealing(const Target& current, bool accept) {
    if (current.shown) {
        return false;
    }
    const bool pressed = accept || std::ranges::any_of(kDirectionKeys, [](const auto& entry) { return ImGui::IsKeyPressed(entry.first, kRepeat, ImGuiKeyOwner_NoOwner); });
    if (!pressed) {
        return false;
    }
    ringVisible = true;
    reveal = current.id;
    setFocus(current.id, current.window, current.bounds);
    return true;
}

// The page actions go to the innermost collection around the focus, which scrolls and moves the focus itself while it draws.
bool FocusNavigator::takePageAction(const NavigationInput& navigation, const Target& current) {
    constexpr std::array<std::pair<NavigationInput::Action, Paging::Kind>, 4> kPages{{
        {NavigationInput::Action::PagePrevious, Paging::Kind::Previous},
        {NavigationInput::Action::PageNext, Paging::Kind::Next},
        {NavigationInput::Action::First, Paging::Kind::First},
        {NavigationInput::Action::Last, Paging::Kind::Last},
    }};
    for (const auto& [action, kind] : kPages) {
        if (!navigation.isPressed(action)) {
            continue;
        }
        if (const std::optional<std::size_t> scope = findCollection(drawn.nodes[current.node].scope)) {
            ringVisible = true;
            pendingPaging = {drawn.scopes[*scope].id, Paging{.kind = kind}};
        }
        return true;
    }
    return false;
}

void FocusNavigator::beginDraw() {
    building.nodes.clear();
    building.scopes.clear();
    building.targets.clear();
    building.guis.clear();
    levels.clear();
    cancelWindows.clear();
    currentGui = nullptr;
    cellOwner = nullptr;
}

void FocusNavigator::beginGui(const Gui& gui) {
    currentGui = &gui;
    building.guis.push_back({.gui = &gui, .window = ImGui::GetCurrentWindow()->RootWindow, .root = 0});
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
    if (!building.guis.empty() && building.guis.back().root == 0) {
        building.guis.back().root = node;
    }
    levels.push_back(level);
}

void FocusNavigator::leave() {
    levels.pop_back();
}

void FocusNavigator::addTarget(ImGuiID item, const math::Rect& bounds) {
    const ImRect& clip = ImGui::GetCurrentWindow()->ClipRect;
    const bool shown = bounds.x < clip.Max.x && bounds.getRight() > clip.Min.x && bounds.y < clip.Max.y && bounds.getBottom() > clip.Min.y;
    addItem(item, bounds, false, shown);
}

void FocusNavigator::addPlayArea(ImGuiID item, const math::Rect& bounds) {
    addItem(item, bounds, true, true);
}

void FocusNavigator::addProxyTarget(ImGuiID item, const math::Rect& bounds) {
    addItem(item, bounds, false, false);
}

void FocusNavigator::addItem(ImGuiID item, const math::Rect& bounds, bool play, bool shown) {
    if (suspended || levels.empty() || !levels.back().focusable || (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0) {
        return;
    }

    // A component becomes a node the first time it registers a target in a frame, so components without targets cost nothing. The targets a collection registers for its cells each name their own item, so each one is a node.
    Level& level = levels.back();
    if (!level.node || (cellOwner != nullptr && level.component == cellOwner)) {
        Node node{.id = level.id, .gui = currentGui, .name = level.component->getId(), .item = {}, .part = {}, .neighbors = level.component->getCommon().focusNeighbors, .usedDirections = 0, .scope = level.scope};
        if (cellOwner != nullptr) {
            node.name = cellOwner->getId();
            node.item = cellItem;
            node.part = level.component == cellOwner ? std::string() : level.component->getId();
        }
        for (const FocusDirection direction : {FocusDirection::Left, FocusDirection::Right, FocusDirection::Up, FocusDirection::Down}) {
            if (level.component->usesFocusDirection(direction)) {
                node.usedDirections |= toBit(direction);
            }
        }
        building.nodes.push_back(std::move(node));
        level.node = building.nodes.size() - 1;
    }
    const bool inputable = GImGui->LastItemData.ID == item && (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Inputable) != 0;
    building.targets.push_back({.id = item, .window = ImGui::GetCurrentWindow(), .bounds = bounds, .node = *level.node, .inputable = inputable, .play = play, .shown = shown});
}

// The collection reuses the scope a focus wrap of its own opened, so its wraps along the axis reach it.
void FocusNavigator::beginCollection(const math::Rect& bounds, bool horizontal, ImGuiID preferred) {
    Level& level = levels.back();
    if (!level.scope || building.scopes[*level.scope].id != level.id) {
        building.scopes.push_back({.id = level.id, .bounds = bounds, .trap = false, .wrap = FocusWrap::None, .parent = level.scope});
        level.scope = building.scopes.size() - 1;
    }
    Scope& scope = building.scopes[*level.scope];
    scope.bounds = bounds;
    scope.collection = true;
    scope.horizontal = horizontal;
    scope.preferred = preferred;
}

std::optional<FocusNavigator::Paging> FocusNavigator::takePaging() {
    if (!pendingPaging || levels.empty() || !levels.back().scope || building.scopes[*levels.back().scope].id != pendingPaging->first) {
        return std::nullopt;
    }
    return std::exchange(pendingPaging, std::nullopt)->second;
}

void FocusNavigator::enterCell(const Component& collection, std::string_view item) {
    cellOwner = &collection;
    cellItem = item;
}

void FocusNavigator::leaveCell() noexcept {
    cellOwner = nullptr;
    cellItem = {};
}

std::optional<ImGuiID> FocusNavigator::findFocusedSince(std::size_t first) const {
    const ImGuiID focused = GImGui->NavId;
    if (focused == 0 || !hasTargetSince(first, focused)) {
        return std::nullopt;
    }
    return focused;
}

bool FocusNavigator::hasTargetSince(std::size_t first, ImGuiID item) const {
    return item != 0 && std::any_of(building.targets.begin() + static_cast<std::ptrdiff_t>(std::min(first, building.targets.size())), building.targets.end(), [item](const Target& target) { return target.id == item; });
}

void FocusNavigator::markShownSince(std::size_t first) noexcept {
    for (std::size_t index = first; index < building.targets.size(); ++index) {
        building.targets[index].shown = true;
    }
}

bool FocusNavigator::focusFirstSince(std::size_t first) {
    if (first >= building.targets.size()) {
        return false;
    }
    const Target target = building.targets[first];
    focus(target.id, target.bounds);
    return true;
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
        if (!hasTargets(focusedGui)) {
            std::erase_if(parked, [this](const auto& entry) { return entry.first == focusedGui; });
            parked.emplace_back(focusedGui, focusedItem);
        }
        restore();
    }
    focusPopup();
    unpark();

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
    focusedPlay = focused != nullptr && focused->play;
    focusedGui = node != nullptr ? node->gui : nullptr;
    focusedName = node != nullptr ? node->name : std::string();
    focusedCellItem = node != nullptr ? node->item : std::string();
    focusedPart = node != nullptr ? node->part : std::string();
    focusedRoot = focused != nullptr ? focused->window->RootWindow : nullptr;
    focusedBounds = focused != nullptr ? focused->bounds : math::Rect{};

    editing = g.ActiveId != 0 && (g.ActiveIdWindow == nullptr || g.ActiveId != g.ActiveIdWindow->MoveId);
    const ImGuiWindow* answering = popupAtEnd != nullptr ? popupAtEnd : focusedRoot;
    cancelAnswered = editing || focusedItem != 0 || popupsAtEnd > 0 || carried.has_value() || std::ranges::find(cancelWindows, answering) != cancelWindows.end();
    acceptAnswered = editing || (focusedItem != 0 && !focusedPlay);
    focusAnswered = focusedItem != 0 || std::ranges::any_of(drawn.targets, &Target::play);
}

FocusNavigator::Owner FocusNavigator::getOwner() const noexcept {
    if (focusedItem == 0) {
        return Owner::None;
    }
    return focusedPlay ? Owner::PlayArea : Owner::Control;
}

void FocusNavigator::focus(ImGuiID item, const math::Rect& bounds) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    reveal = item;
    ringVisible = ringVisible || lastInput == input::InputDevice::Gamepad || !pointerAvailable;
    if (const Target* target = find(building, item)) {
        remember(building, *target);
        keepControl(*target);
    }
    setFocus(item, window, bounds);
    focusedGui = currentGui;
    focusedRoot = window->RootWindow;
    focusedBounds = bounds;
}

void FocusNavigator::forget(const Gui& gui) {
    std::erase_if(parked, [&gui](const auto& entry) { return entry.first == &gui; });
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
    return g.NavId != 0 && focusedGui == currentGui && focusedRoot == ImGui::GetCurrentWindow()->RootWindow;
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

const Gui* FocusNavigator::getFocusedGui() const noexcept {
    return focusedItem != 0 ? focusedGui : nullptr;
}

std::string_view FocusNavigator::getFocusedName() const noexcept {
    return focusedName;
}

std::string_view FocusNavigator::getFocusedItem() const noexcept {
    return focusedCellItem;
}

std::string_view FocusNavigator::getFocusedPart() const noexcept {
    return focusedPart;
}

const FocusNavigator::Target* FocusNavigator::find(const Frame& frame, ImGuiID item) const noexcept {
    if (item == 0) {
        return nullptr;
    }
    const auto found = std::ranges::find(frame.targets, item, &Target::id);
    return found != frame.targets.end() ? &*found : nullptr;
}

const FocusNavigator::Target* FocusNavigator::findNamed(const Gui* gui, std::string_view name) const noexcept {
    const auto found = std::ranges::find_if(drawn.targets, [&](const Target& target) { return drawn.nodes[target.node].gui == gui && drawn.nodes[target.node].name == name; });
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

// A GUI is a navigation space of its own, and GUIs that share the focus form one together.
bool FocusNavigator::isSameSpace(const Gui* first, const Gui* second) noexcept {
    return first == second || (first->hasSharedFocus() && second->hasSharedFocus());
}

// Peers share the navigation space and the innermost focus scope, so the focus never enters a scope it is not in, and a popup keeps the focus inside it.
bool FocusNavigator::isPeer(const Target& source, const Target& target) const noexcept {
    const Node& origin = drawn.nodes[source.node];
    const Node& node = drawn.nodes[target.node];
    const bool popup = ((source.window->RootWindow->Flags | target.window->RootWindow->Flags) & ImGuiWindowFlags_Popup) != 0;
    return isSameSpace(node.gui, origin.gui) && (target.window->RootWindow == source.window->RootWindow || !popup) && findTrap(node.scope) == findTrap(origin.scope);
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

// A node that wraps along the direction keeps the move inside it and starts again from its other side when nothing lies ahead. A collection does not draw every item, so it answers a wrap along its axis itself.
const FocusNavigator::Target* FocusNavigator::searchAround(const Target& source, FocusDirection direction) {
    const Node& node = drawn.nodes[source.node];
    const std::optional<std::size_t> trap = findTrap(node.scope);
    const std::optional<std::size_t> wrap = findWrap(node.scope, trap, direction);
    if (!wrap) {
        return search(source, source.bounds, trap, direction);
    }
    if (const Target* ahead = search(source, source.bounds, wrap, direction)) {
        return ahead;
    }
    const Scope& scope = drawn.scopes[*wrap];
    const bool horizontal = direction == FocusDirection::Left || direction == FocusDirection::Right;
    if (scope.collection && horizontal == scope.horizontal) {
        pendingPaging = {scope.id, Paging{.kind = Paging::Kind::Wrap, .direction = direction}};
        return nullptr;
    }
    return search(source, FocusSearch::wrap(source.bounds, scope.bounds, direction), wrap, direction);
}

std::optional<std::size_t> FocusNavigator::findCollection(std::optional<std::size_t> scope) const noexcept {
    while (scope && !drawn.scopes[*scope].collection) {
        scope = drawn.scopes[*scope].parent;
    }
    return scope;
}

// A move that enters a collection from outside lands on the item that had the focus there last, when the collection remembers it and draws it.
const FocusNavigator::Target& FocusNavigator::prefer(const Target& source, const Target& target) const noexcept {
    const std::optional<std::size_t> scope = findCollection(drawn.nodes[target.node].scope);
    if (!scope || drawn.scopes[*scope].preferred == 0 || isInside(source, scope)) {
        return target;
    }
    const Target* preferred = find(drawn, drawn.scopes[*scope].preferred);
    return preferred != nullptr ? *preferred : target;
}

// The play area of the GUI that holds the focus, or of the topmost GUI that draws one while nothing has the focus.
const FocusNavigator::Target* FocusNavigator::findPlayArea(const Target* source) const noexcept {
    const Gui* gui = source != nullptr ? drawn.nodes[source->node].gui : nullptr;
    const Target* found = nullptr;
    for (const Target& target : drawn.targets) {
        if (target.play && (gui == nullptr || drawn.nodes[target.node].gui == gui)) {
            found = &target;
            if (gui != nullptr) {
                break;
            }
        }
    }
    return found;
}

// The control that had the focus before the play area took it, or else the first control of the GUI of the play area.
const FocusNavigator::Target* FocusNavigator::findControl(const Target& playArea) const noexcept {
    const Gui* gui = drawn.nodes[playArea.node].gui;
    if (const Target* remembered = find(drawn, returnControl); remembered != nullptr && drawn.nodes[remembered->node].gui == gui) {
        return remembered;
    }
    const auto first = std::ranges::find_if(drawn.targets, [&](const Target& target) { return !target.play && drawn.nodes[target.node].gui == gui; });
    return first != drawn.targets.end() ? &*first : nullptr;
}

ImGuiID FocusNavigator::getTrapId(const Frame& frame, const Target& target) const noexcept {
    for (std::optional<std::size_t> scope = frame.nodes[target.node].scope; scope; scope = frame.scopes[*scope].parent) {
        if (frame.scopes[*scope].trap) {
            return frame.scopes[*scope].id;
        }
    }
    return 0;
}

// The action `uiFocus` moves the focus from a play area to the controls of its GUI and back, showing the ring where it lands.
void FocusNavigator::switchFocus(const Target* source) {
    const Target* target = source != nullptr && source->play ? findControl(*source) : findPlayArea(source);
    if (target == nullptr) {
        return;
    }
    ringVisible = true;
    apply(*target);
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
        if (const Target* target = findNamed(node.gui, neighbor)) {
            apply(*target);
            return;
        }
    }
    if (const Target* target = searchAround(source, direction)) {
        apply(prefer(source, *target));
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
    reveal = target.id;
    remember(drawn, target);
    keepControl(target);
    setFocus(target.id, target.window, target.bounds);
    focusedGui = drawn.nodes[target.node].gui;
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

// Cancel goes to the innermost focus scope around the focus, or to the root of the GUI that holds it, or of the topmost GUI when nothing has the focus. Focus in a window of its own, such as a draggable window, leaves cancel to that window.
void FocusNavigator::cancel(const Target* source) {
    const Node* node = source != nullptr ? &drawn.nodes[source->node] : nullptr;
    if (node != nullptr) {
        if (const std::optional<std::size_t> trap = findTrap(node->scope)) {
            notify(drawn.scopes[*trap].id, "cancel");
            return;
        }
    }
    if (drawn.guis.empty()) {
        return;
    }
    const Gui* gui = node != nullptr ? node->gui : drawn.guis.back().gui;
    const auto entry = std::ranges::find(drawn.guis, gui, &GuiEntry::gui);
    if (entry != drawn.guis.end() && entry->root != 0 && (source == nullptr || source->window->RootWindow == entry->window)) {
        notify(entry->root, "cancel");
    }
}

// A move into another navigation space, ImGui window or focus scope remembers where the focus was, so it goes back there when the scope closes.
void FocusNavigator::remember(const Frame& frame, const Target& next) {
    const Target* current = find(drawn, GImGui->NavId);
    if (current == nullptr || current->id == next.id) {
        return;
    }
    const bool elsewhere = !isSameSpace(drawn.nodes[current->node].gui, frame.nodes[next.node].gui) || current->window->RootWindow != next.window->RootWindow || getTrapId(drawn, *current) != getTrapId(frame, next);
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
// A move from a control into a play area keeps the control, where `uiFocus` takes the focus back.
void FocusNavigator::keepControl(const Target& next) {
    const Target* current = find(drawn, GImGui->NavId);
    if (next.play && current != nullptr && !current->play) {
        returnControl = current->id;
    }
}

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

bool FocusNavigator::hasTargets(const Gui* gui) const noexcept {
    return std::ranges::any_of(drawn.targets, [this, gui](const Target& target) { return drawn.nodes[target.node].gui == gui; });
}

// A GUI that draws its controls again gives the focus back to the control it parked, unless the focus went elsewhere meanwhile.
void FocusNavigator::unpark() {
    ImGuiContext& g = *GImGui;
    for (auto entry = parked.begin(); entry != parked.end();) {
        if (!hasTargets(entry->first)) {
            ++entry;
            continue;
        }
        const Target* target = find(drawn, entry->second);
        if (target != nullptr && find(drawn, g.NavId) == nullptr) {
            setFocus(target->id, target->window, target->bounds);
        }
        entry = parked.erase(entry);
    }
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
