#include "haylen/ui/Navigation.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/platform/Accessibility.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/NavigationStack.hpp"
#include "haylen/ui/Screen.hpp"

namespace haylen::ui {

Navigation::Navigation(core::Engine& owner, plugins::UiPlugin& plugin) : engine(owner), ui(plugin) {}

Navigation::~Navigation() = default;

std::shared_ptr<NavigationStack> Navigation::createStack(Region region, std::shared_ptr<Screen> root) {
    checkRegion(region);
    auto stack = std::make_shared<NavigationStack>(*this, std::move(region));
    stacks.push_back(stack);
    if (!root) {
        return stack;
    }

    try {
        stack->push(std::move(root), {.effect = NavigationTransition::Effect::None});
    } catch (...) {
        stack->close();
        throw;
    }

    return stack;
}

std::shared_ptr<NavigationTabs> Navigation::createTabs(Region region, std::vector<NavigationTabs::Tab> list, std::string_view selected) {
    checkRegion(region);
    auto created = std::make_shared<NavigationTabs>(*this, std::move(region), std::move(list), selected);
    tabs.push_back(created);
    try {
        static_cast<void>(created->getCurrent());
    } catch (...) {
        created->close();
        throw;
    }

    return created;
}

std::shared_ptr<NavigationSplit> Navigation::createSplit(Region primary, Region detail, std::shared_ptr<Screen> root, NavigationSplit::Factory placeholder, bool collapsed) {
    checkRegion(primary);
    checkRegion(detail);
    auto created = std::make_shared<NavigationSplit>(*this, std::move(primary), std::move(detail), std::move(root), std::move(placeholder), collapsed);
    splits.push_back(created);
    return created;
}

std::shared_ptr<Presentation> Navigation::present(std::shared_ptr<Screen> screen, Presentation::Options options) {
    auto created = std::make_shared<Presentation>(*this, std::move(screen), options);
    presentations.push_back(created);
    created->open();
    return created;
}

void Navigation::checkRegion(const Region& region) const {
    ui.checkRegion(region);
}

bool Navigation::isMotionReduced() const {
    return engine.getAccessibility().getSettings().reducedMotion;
}

// Hooks run in the lifecycle phase, so their failures restart the app in development like the failures of scene hooks.
template <typename Hook> void Navigation::notify(Screen& screen, Hook&& hook) {
    const core::Engine::PhaseScope scope(engine, core::Engine::Phase::Lifecycle);
    lua::Runtime::runReporting(engine.getLuaState(), [&] { hook(screen); });
}

bool Navigation::askBack(Screen& screen) {
    bool kept = false;
    notify(screen, [&](Screen& target) { kept = target.back(engine); });
    return kept;
}

void Navigation::place(Screen& screen, const std::shared_ptr<NavigationStack>& stack) {
    screen.stack = stack;
    screen.presentation.reset();
    if (screen.state == Screen::State::Removed) {
        screen.state = Screen::State::Created;
    }

    if (screen.gui) {
        ui.moveScreen(*screen.gui, stack->getRegion());
    }

    notify(screen, [&](Screen& target) { target.placed(engine); });
}

void Navigation::place(Screen& screen, const std::shared_ptr<Presentation>& presentation) {
    screen.presentation = presentation;
    screen.stack.reset();
    if (screen.state == Screen::State::Removed) {
        screen.state = Screen::State::Created;
    }

    notify(screen, [&](Screen& target) { target.placed(engine); });
}

// The screen of a stack lays out in the area of its stack, and the screen of a presentation in the area that the presentation gives it.
void Navigation::mountScreen(Screen& screen) {
    if (screen.gui) {
        return;
    }

    const std::shared_ptr<NavigationStack> stack = screen.stack.lock();
    const Placement placement = stack ? Placement::Region : Placement::Screen;
    // clang-format off
    notify(screen, [&](Screen& target) {
        std::shared_ptr<Gui> built = target.build(engine, placement);
        if (!built || built->getPlacement() != placement) {
            throw std::logic_error("A screen builds a GUI with the placement that the navigation gives it.");
        }

        target.gui = std::move(built);
        ui.mountScreen(target, stack ? &stack->getRegion() : nullptr);
    });
    // clang-format on
}

void Navigation::unmountScreen(Screen& screen) {
    const std::shared_ptr<Gui> gui = std::exchange(screen.gui, nullptr);
    if (gui) {
        ui.unmount(*gui);
    }
}

void Navigation::beginAppear(Screen& screen) {
    mountScreen(screen);
    screen.state = Screen::State::Appearing;
    notify(screen, [&](Screen& target) { target.appear(engine); });
    showHosted(screen, true);
}

void Navigation::endAppear(Screen& screen) {
    screen.state = Screen::State::Shown;
    notify(screen, [&](Screen& target) { target.appeared(engine); });
}

void Navigation::beginDisappear(Screen& screen) {
    screen.state = Screen::State::Disappearing;
    notify(screen, [&](Screen& target) { target.disappear(engine); });
}

void Navigation::endDisappear(Screen& screen) {
    screen.state = Screen::State::Hidden;
    notify(screen, [&](Screen& target) { target.disappeared(engine); });
    showHosted(screen, false);
}

void Navigation::showAtOnce(Screen& screen) {
    if (screen.state == Screen::State::Disappearing) {
        endDisappear(screen);
    }
    if (screen.state == Screen::State::Created || screen.state == Screen::State::Hidden) {
        beginAppear(screen);
    }
    if (screen.state == Screen::State::Appearing) {
        endAppear(screen);
    }
}

void Navigation::hideAtOnce(Screen& screen) {
    if (screen.state == Screen::State::Appearing) {
        endAppear(screen);
    }
    if (screen.state == Screen::State::Shown) {
        beginDisappear(screen);
    }
    if (screen.state == Screen::State::Disappearing) {
        endDisappear(screen);
    }
}

void Navigation::removeScreen(Screen& screen) {
    screen.state = Screen::State::Removed;
    screen.stack.reset();
    screen.presentation.reset();
    notify(screen, [&](Screen& target) { target.remove(engine); });
    unmountScreen(screen);
}

// The stacks in the GUI of a screen show their top screens while that screen shows, and hide them once it no longer shows.
void Navigation::showHosted(const Screen& screen, bool value) {
    if (!screen.gui) {
        return;
    }

    std::vector<std::shared_ptr<NavigationStack>> hosted;
    for (const std::shared_ptr<NavigationStack>& stack : stacks) {
        if (stack->getRegion().gui == screen.gui && stack->isShown() && !stack->empty()) {
            hosted.push_back(stack);
        }
    }

    for (const std::shared_ptr<NavigationStack>& stack : hosted) {
        const std::shared_ptr<Screen> top = stack->getTop();
        if (!top) {
            continue;
        }

        if (value) {
            showAtOnce(*top);
        } else {
            hideAtOnce(*top);
        }
    }
}

const Screen* Navigation::findHostScreen(const NavigationStack& stack) const {
    const Gui* gui = stack.getRegion().gui.get();
    for (const std::shared_ptr<NavigationStack>& candidate : stacks) {
        for (const std::shared_ptr<Screen>& screen : candidate->getScreens()) {
            if (screen->gui.get() == gui) {
                return screen.get();
            }
        }
    }

    for (const std::shared_ptr<Presentation>& presentation : presentations) {
        if (presentation->getScreen()->gui.get() == gui) {
            return presentation->getScreen().get();
        }
    }

    return nullptr;
}

bool Navigation::isHostShowing(const NavigationStack& stack) const {
    const Screen* host = findHostScreen(stack);
    if (host == nullptr) {
        return true;
    }

    const Screen::State state = host->getState();
    return state == Screen::State::Appearing || state == Screen::State::Shown || state == Screen::State::Disappearing;
}

bool Navigation::isDrawn(const NavigationStack& stack) const {
    if (!stack.isActive()) {
        return false;
    }

    if (const Screen* host = findHostScreen(stack)) {
        return isDrawn(*host);
    }

    const std::shared_ptr<Gui>& gui = stack.getRegion().gui;
    return gui->isVisible() && ui.isMounted(*gui);
}

bool Navigation::isDrawn(const Screen& screen) const {
    const Screen::State state = screen.getState();
    if (state != Screen::State::Appearing && state != Screen::State::Shown && state != Screen::State::Disappearing) {
        return false;
    }

    if (screen.presentation.lock()) {
        return true;
    }

    const std::shared_ptr<NavigationStack> stack = screen.stack.lock();
    return stack && isDrawn(*stack);
}

std::size_t Navigation::getDepth(const NavigationStack& stack) const {
    std::size_t depth = 0;
    const Screen* host = findHostScreen(stack);
    while (host != nullptr) {
        ++depth;
        const std::shared_ptr<NavigationStack> outer = host->stack.lock();
        host = outer ? findHostScreen(*outer) : nullptr;
    }

    return depth;
}

const Presentation* Navigation::findPresentation(const NavigationStack& stack) const {
    const Screen* host = findHostScreen(stack);
    while (host != nullptr) {
        if (const std::shared_ptr<Presentation> presentation = host->presentation.lock()) {
            return presentation.get();
        }

        const std::shared_ptr<NavigationStack> outer = host->stack.lock();
        host = outer ? findHostScreen(*outer) : nullptr;
    }

    return nullptr;
}

const Presentation* Navigation::getTopPresentation() const {
    for (auto presentation = presentations.rbegin(); presentation != presentations.rend(); ++presentation) {
        if (!(*presentation)->isDismissed()) {
            return presentation->get();
        }
    }

    return nullptr;
}

// The innermost stacks come first, and among stacks as deep, the ones created last, such as the detail of a split before its list.
void Navigation::collectActive(std::vector<NavigationStack*>& found) const {
    const Presentation* layer = getTopPresentation();
    found.clear();
    for (auto stack = stacks.rbegin(); stack != stacks.rend(); ++stack) {
        if (isDrawn(**stack) && findPresentation(**stack) == layer) {
            found.push_back(stack->get());
        }
    }

    std::ranges::stable_sort(found, [this](const NavigationStack* lhs, const NavigationStack* rhs) { return getDepth(*lhs) > getDepth(*rhs); });
}

bool Navigation::canGoBack() const {
    if (getTopPresentation() != nullptr) {
        return true;
    }

    collectActive(activeStacks);
    const bool pops = std::ranges::any_of(activeStacks, &NavigationStack::canGoBack);
    activeStacks.clear();
    if (pops) {
        return true;
    }

    // clang-format off
    return std::ranges::any_of(tabs, [this](const std::shared_ptr<NavigationTabs>& candidate) {
        const NavigationTabs::Entry& current = candidate->find(candidate->selected);
        return current.stack && isDrawn(*current.stack) && candidate->entries.front().tab.id != candidate->selected;
    });
    // clang-format on
}

bool Navigation::goBack() {
    const Presentation* layer = getTopPresentation();
    collectActive(activeStacks);
    std::shared_ptr<NavigationStack> popping;
    for (NavigationStack* stack : activeStacks) {
        if (stack->canGoBack()) {
            popping = stack->shared_from_this();
            break;
        }
    }

    activeStacks.clear();
    if (popping) {
        const std::shared_ptr<Screen> top = popping->getTop();
        if (!askBack(*top) && !popping->isClosed()) {
            popping->pop();
        }

        return true;
    }

    for (auto candidate = tabs.rbegin(); candidate != tabs.rend(); ++candidate) {
        const std::shared_ptr<NavigationTabs> held = *candidate;
        const NavigationTabs::Entry& current = held->find(held->selected);
        if (current.stack && isDrawn(*current.stack) && findPresentation(*current.stack) == layer && held->goBackToFirst()) {
            return true;
        }
    }

    if (layer == nullptr) {
        return false;
    }

    const std::shared_ptr<Presentation> top = std::const_pointer_cast<Presentation>(layer->shared_from_this());
    if (top->isDismissible() && !askBack(*top->getScreen())) {
        top->dismiss();
    }

    return true;
}

Navigation::Look Navigation::getLook(const Screen& screen) const {
    Look look;
    const Screen::State state = screen.getState();
    if (state != Screen::State::Appearing && state != Screen::State::Shown && state != Screen::State::Disappearing) {
        return look;
    }

    // A presentation that slides comes in from the start of the reading direction, the way a drawer opens.
    if (const std::shared_ptr<Presentation> presentation = screen.presentation.lock()) {
        look.shown = true;
        look.inert = presentation->phase != Presentation::Phase::Shown;
        look.effect = presentation->transition.effect;
        look.distance = presentation->getDistance();
        look.side = -1.0F;
        return look;
    }

    const std::shared_ptr<NavigationStack> stack = screen.stack.lock();
    if (!stack || !stack->isActive()) {
        return look;
    }

    const bool incoming = stack->change && stack->change->incoming.get() == &screen;
    const bool outgoing = stack->change && stack->change->outgoing.get() == &screen;
    if (!incoming && !outgoing) {
        look.shown = stack->getTop().get() == &screen;
        return look;
    }

    const NavigationStack::Change& change = *stack->change;
    const float progress = stack->getProgress();
    look.shown = true;
    look.inert = true;
    look.distance = incoming ? 1.0F - progress : progress;
    look.effect = change.transition.effect;
    if (look.effect == NavigationTransition::Effect::Slide) {
        look.side = incoming == change.backward ? -1.0F : 1.0F;
    }

    // A rise moves only the screen above, the one that comes or the one that goes back down, while the screen under it fades.
    const bool above = incoming != change.backward;
    if (look.effect == NavigationTransition::Effect::Rise && !above) {
        look.effect = NavigationTransition::Effect::Fade;
    }

    return look;
}

void Navigation::update(float deltaSeconds) {
    walkedStacks.assign(stacks.begin(), stacks.end());
    for (const std::shared_ptr<NavigationStack>& stack : walkedStacks) {
        stack->advance(deltaSeconds);
    }

    walkedStacks.clear();
    walkedPresentations.assign(presentations.begin(), presentations.end());
    for (const std::shared_ptr<Presentation>& presentation : walkedPresentations) {
        presentation->advance(deltaSeconds);
    }

    walkedPresentations.clear();
    collectDrawn(walkedScreens);
    for (const std::shared_ptr<Screen>& screen : walkedScreens) {
        if (isDrawn(*screen)) {
            lua::Runtime::runReporting(engine.getLuaState(), [&] { screen->update(engine, deltaSeconds); });
        }
    }

    walkedScreens.clear();
}

void Navigation::render() {
    collectDrawn(walkedScreens);
    for (const std::shared_ptr<Screen>& screen : walkedScreens) {
        if (isDrawn(*screen)) {
            lua::Runtime::runReporting(engine.getLuaState(), [&] { screen->render(engine); });
        }
    }

    walkedScreens.clear();
}

// The screens that draw are the top screens of the stacks that draw, with both screens of a change that plays, and the screens of the presentations.
void Navigation::collectDrawn(std::vector<std::shared_ptr<Screen>>& found) const {
    found.clear();
    for (const std::shared_ptr<NavigationStack>& stack : stacks) {
        if (!isDrawn(*stack)) {
            continue;
        }

        if (stack->change && stack->change->outgoing) {
            found.push_back(stack->change->outgoing);
        }
        if (!stack->empty()) {
            found.push_back(stack->getTop());
        }
    }

    for (const std::shared_ptr<Presentation>& presentation : presentations) {
        found.push_back(presentation->getScreen());
    }
}

void Navigation::closeHostedBy(const Gui& gui) {
    std::vector<std::shared_ptr<NavigationTabs>> closingTabs;
    std::vector<std::shared_ptr<NavigationSplit>> closingSplits;
    std::vector<std::shared_ptr<NavigationStack>> closingStacks;
    for (const std::shared_ptr<NavigationTabs>& candidate : tabs) {
        if (candidate->getRegion().gui.get() == &gui) {
            closingTabs.push_back(candidate);
        }
    }

    for (const std::shared_ptr<NavigationSplit>& candidate : splits) {
        if (candidate->getPrimary()->getRegion().gui.get() == &gui || candidate->getDetail()->getRegion().gui.get() == &gui) {
            closingSplits.push_back(candidate);
        }
    }

    for (const std::shared_ptr<NavigationStack>& candidate : stacks) {
        if (candidate->getRegion().gui.get() == &gui) {
            closingStacks.push_back(candidate);
        }
    }

    for (const std::shared_ptr<NavigationTabs>& closing : closingTabs) {
        closing->close();
    }

    for (const std::shared_ptr<NavigationSplit>& closing : closingSplits) {
        closing->close();
    }

    for (const std::shared_ptr<NavigationStack>& closing : closingStacks) {
        closing->close();
    }
}

// The UI stops without running hooks, since the app ends with it.
void Navigation::closeAll() {
    for (const std::shared_ptr<NavigationTabs>& candidate : tabs) {
        candidate->closed = true;
        candidate->selectionChanged.clear();
    }

    for (const std::shared_ptr<NavigationSplit>& candidate : splits) {
        candidate->closed = true;
        candidate->primaryChanged.disconnect();
    }

    for (const std::shared_ptr<NavigationStack>& stack : stacks) {
        stack->closed = true;
        stack->change.reset();
        stack->screens.clear();
        stack->changed.clear();
    }

    for (const std::shared_ptr<Presentation>& presentation : presentations) {
        presentation->phase = Presentation::Phase::Gone;
        presentation->dismissed.clear();
    }

    tabs.clear();
    splits.clear();
    stacks.clear();
    presentations.clear();
}

void Navigation::forget(const NavigationStack& stack) {
    std::erase_if(stacks, [&](const std::shared_ptr<NavigationStack>& entry) { return entry.get() == &stack; });
}

void Navigation::forget(const NavigationTabs& closed) {
    std::erase_if(tabs, [&](const std::shared_ptr<NavigationTabs>& entry) { return entry.get() == &closed; });
}

void Navigation::forget(const NavigationSplit& split) {
    std::erase_if(splits, [&](const std::shared_ptr<NavigationSplit>& entry) { return entry.get() == &split; });
}

void Navigation::forget(const Presentation& presentation) {
    std::erase_if(presentations, [&](const std::shared_ptr<Presentation>& entry) { return entry.get() == &presentation; });
}

} // namespace haylen::ui
