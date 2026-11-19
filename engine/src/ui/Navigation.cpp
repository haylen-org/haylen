#include "haylen/ui/Navigation.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/platform/Accessibility.hpp"
#include "haylen/plugins/StoragePlugin.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/storage/ResumeState.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/NavigationStack.hpp"
#include "haylen/ui/Screen.hpp"
#include "ui/AlertScreen.hpp"
#include "ui/MessageContent.hpp"

namespace haylen::ui {

// clang-format off
Navigation::Navigation(core::Engine& owner, plugins::UiPlugin& plugin) : engine(owner), ui(plugin) {
    linkConnection = core::ScopedConnection(engine.getEvents().on(core::LifecycleEvent::kLinkOpened, [this](core::EventBus::Event& event) {
        launchLink = event.getData().value("url", std::string());
    }));
}
// clang-format on

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

    // Showing a restored detail runs the hooks of the app, which may create or close routers.
    const std::vector<std::shared_ptr<Router>> waiting = routers;
    for (const std::shared_ptr<Router>& router : waiting) {
        router->showWaitingDetails(*created);
    }

    return created;
}

// A modal presentation keeps the focus of the GUIs under it, which goes back where it was once the presentation left.
std::shared_ptr<Presentation> Navigation::present(std::shared_ptr<Screen> screen, Presentation::Options options) {
    auto created = std::make_shared<Presentation>(*this, std::move(screen), std::move(options));
    if (created->isModal()) {
        ui.getFocus().parkFocus();
    }

    presentations.push_back(created);
    created->open();
    return created;
}

const Navigation::Alert::Action* Navigation::Alert::findCancel() const noexcept {
    const auto found = std::ranges::find(actions, Role::Cancel, &Action::role);
    return found != actions.end() ? &*found : nullptr;
}

// A router that keeps the state of the navigation builds again what the run before it kept, and a router whose state fails to build closes, so the links keep waiting for a router that opens them.
std::shared_ptr<Router> Navigation::createRouter(Router::Options options) {
    const bool restores = options.restores;
    const bool links = options.links;
    auto created = std::make_shared<Router>(*this, engine, std::move(options));
    routers.push_back(created);

    try {
        if (restores) {
            restoreState(*created);
        }
    } catch (...) {
        created->close();
        throw;
    }

    if (links && !routed) {
        routed = true;
        linkConnection.disconnect();
    }

    return created;
}

void Navigation::restoreState(Router& router) {
    const std::optional<storage::ResumeState::Restored>& restored = engine.getPlugin<plugins::StoragePlugin>().getNavigationState().getRestored();
    if (restored) {
        router.restore(restored->value);
    }
}

std::shared_ptr<NavigationSplit> Navigation::findSplit(const Screen& host) const {
    const auto found = std::ranges::find_if(splits, [&host](const std::shared_ptr<NavigationSplit>& split) { return host.getGui() && split->getPrimary()->getRegion().gui == host.getGui(); });
    return found != splits.end() ? *found : nullptr;
}

// An alert fades in as a dialog, which the person leaves only through its cancel action. An action sheet points at its anchor as a popover on a wide screen, and rises as a sheet as tall as its actions on a compact one or without an anchor, and a tap outside it cancels it.
std::shared_ptr<Presentation> Navigation::presentAlert(Alert alert) {
    auto screen = std::make_shared<AlertScreen>(ui, std::move(alert));
    const Alert& shown = screen->getAlert();
    Presentation::Options options{.dismissible = shown.findCancel() != nullptr};
    if (shown.style == Alert::Style::Alert) {
        options.style = Presentation::Style::Dialog;
        return present(std::move(screen), std::move(options));
    }

    options.style = shown.anchor ? Presentation::Style::Popover : Presentation::Style::Sheet;
    options.dismissible = true;
    options.anchor = shown.anchor;
    options.detents = {{.kind = Presentation::Detent::Kind::Fit}};
    options.width = Alert::kActionSheetWidth;
    return present(std::move(screen), std::move(options));
}

std::shared_ptr<Message> Navigation::showMessage(const core::Json& properties) {
    auto created = std::make_shared<Message>(properties);
    messages.push_back(created);
    return created;
}

void Navigation::setMessageArea(std::optional<Region> value) {
    if (value) {
        checkRegion(*value);
    }

    messageArea = std::move(value);
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

bool Navigation::isReady(const Screen& screen) const {
    return !screen.gui || ui.isComplete(*screen.gui);
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

const Presentation* Navigation::getTopModal() const {
    for (auto presentation = presentations.rbegin(); presentation != presentations.rend(); ++presentation) {
        if (!(*presentation)->isDismissed() && (*presentation)->isModal()) {
            return presentation->get();
        }
    }

    return nullptr;
}

// The innermost stacks come first, and among stacks as deep, the ones created last, such as the detail of a split before its list. Each stack goes in its place as the walk finds it, so the list that the caller keeps between frames allocates nothing.
void Navigation::collectActive(const Presentation* layer, std::vector<NavigationStack*>& found) const {
    found.clear();
    for (auto stack = stacks.rbegin(); stack != stacks.rend(); ++stack) {
        if (!isDrawn(**stack) || findPresentation(**stack) != layer) {
            continue;
        }

        const std::size_t depth = getDepth(**stack);
        const auto shallower = std::ranges::find_if(found, [this, depth](const NavigationStack* other) { return getDepth(*other) < depth; });
        found.insert(shallower, stack->get());
    }
}

bool Navigation::canGoBackIn(const Presentation* layer) const {
    collectActive(layer, activeStacks);
    const bool pops = std::ranges::any_of(activeStacks, &NavigationStack::canGoBack);
    activeStacks.clear();
    if (pops) {
        return true;
    }

    // clang-format off
    return std::ranges::any_of(tabs, [this, layer](const std::shared_ptr<NavigationTabs>& candidate) {
        const NavigationTabs::Entry& current = candidate->find(candidate->selected);
        return current.stack && isDrawn(*current.stack) && findPresentation(*current.stack) == layer && candidate->entries.front().tab.id != candidate->selected;
    });
    // clang-format on
}

bool Navigation::goBackIn(const Presentation* layer) {
    collectActive(layer, activeStacks);
    std::shared_ptr<NavigationStack> popping;
    for (NavigationStack* stack : activeStacks) {
        if (stack->canGoBack()) {
            popping = stack->shared_from_this();
            break;
        }
    }

    activeStacks.clear();
    if (popping) {
        // A hook that changed the stack itself took the back as well, such as one that pops with a transition of its own.
        const std::shared_ptr<Screen> top = popping->getTop();
        const bool kept = askBack(*top);
        if (!kept && !popping->isClosed() && popping->getTop() == top) {
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

    return false;
}

// A modal presentation keeps the back from the layers under it, even while the person cannot dismiss it.
bool Navigation::canGoBack() const {
    for (auto presentation = presentations.rbegin(); presentation != presentations.rend(); ++presentation) {
        const Presentation& layer = **presentation;
        if (layer.isDismissed()) {
            continue;
        }

        if (layer.isModal() || layer.canLowerDetent() || canGoBackIn(&layer)) {
            return true;
        }
    }

    return canGoBackIn(nullptr);
}

// The layers are a copy, since the hooks that the back asks may present or dismiss.
bool Navigation::goBack() {
    const std::vector<std::shared_ptr<Presentation>> layers(presentations.rbegin(), presentations.rend());
    for (const std::shared_ptr<Presentation>& layer : layers) {
        if (layer->isDismissed()) {
            continue;
        }

        if (goBackIn(layer.get())) {
            return true;
        }

        if (layer->isModal()) {
            static_cast<void>(layer->dismissByPerson());
            return true;
        }

        if (layer->canLowerDetent()) {
            layer->setDetent(0);
            return true;
        }
    }

    return goBackIn(nullptr);
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
    followLaunchLink();

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
    updateMessages();
}

// The link that launched the app reaches the first router that opens links and still shows once the app registered its handlers, after the router built the state it restored.
void Navigation::followLaunchLink() {
    if (!launchLink || !routed) {
        return;
    }

    const auto found = std::ranges::find_if(routers, [](const std::shared_ptr<Router>& router) { return router->options.links && !router->isClosed() && router->isShowing(); });
    const std::string link = *std::exchange(launchLink, std::nullopt);
    if (found == routers.end()) {
        return;
    }

    const std::shared_ptr<Router> router = *found;
    router->follow(link);
}

// The handlers of the app may show and dismiss messages, so the pass walks a copy.
void Navigation::updateMessages() {
    replaceMessages();
    walkedMessages.assign(messages.begin(), messages.end());
    for (const std::shared_ptr<Message>& message : walkedMessages) {
        if (message->shownOnce && !message->appearReported) {
            message->appearReported = true;
            message->appeared.emit(*message);
        }

        if (message->reason && !message->leaveReported) {
            message->leaveReported = true;
            if (*message->reason == Message::Reason::Action) {
                message->actionPressed.emit(*message);
            }

            if (*message->reason == Message::Reason::Tapped) {
                message->tapped.emit(*message);
            }
        }

        if (message->phase == Message::Phase::Gone) {
            forget(*message);
            message->dismissed.emit(*message);
        }
    }

    walkedMessages.clear();
}

// The one that shows leaves first, and the one that replaces it comes on screen once the stack has room for it.
void Navigation::replaceMessages() {
    for (const std::shared_ptr<Message>& waiting : messages) {
        if (waiting->phase != Message::Phase::Waiting || waiting->style == Message::Style::Toast) {
            continue;
        }

        for (const std::shared_ptr<Message>& shown : messages) {
            const bool rival = shown->phase == Message::Phase::Shown && shown->style == waiting->style && shown->content->position == waiting->content->position;
            if (rival && shown->priority < waiting->priority) {
                shown->leave(Message::Reason::Replaced);
            }
        }
    }
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
    if (messageArea && messageArea->gui.get() == &gui) {
        messageArea.reset();
    }

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

// The UI stops without running hooks, since the app ends with it, and a router that keeps the state of the navigation keeps it first.
void Navigation::closeAll() {
    const std::vector<std::shared_ptr<Router>> closing = std::exchange(routers, {});
    for (const std::shared_ptr<Router>& router : closing) {
        router->close();
    }

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

    for (const std::shared_ptr<Message>& message : messages) {
        message->phase = Message::Phase::Gone;
        message->appeared.clear();
        message->actionPressed.clear();
        message->tapped.clear();
        message->dismissed.clear();
    }

    tabs.clear();
    splits.clear();
    stacks.clear();
    presentations.clear();
    messages.clear();
    messageArea.reset();
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

void Navigation::forget(const Router& router) {
    std::erase_if(routers, [&](const std::shared_ptr<Router>& entry) { return entry.get() == &router; });
}

void Navigation::forget(const Presentation& presentation) {
    std::erase_if(presentations, [&](const std::shared_ptr<Presentation>& entry) { return entry.get() == &presentation; });
}

void Navigation::forget(const Message& message) {
    std::erase_if(messages, [&](const std::shared_ptr<Message>& entry) { return entry.get() == &message; });
}

} // namespace haylen::ui
