#include "haylen/plugins/UiPlugin.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <exception>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/EmbeddedFiles.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/core/SceneView.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/input/ActionMaps.hpp"
#include "haylen/input/AppTextField.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/localization/Catalog.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/platform/Accessibility.hpp"
#include "haylen/plugins/LocalizationPlugin.hpp"
#include "haylen/plugins/TextPlugin.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "haylen/ui/Screen.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/ImGuiLua.hpp"
#include "ui/ImageLibrary.hpp"
#include "ui/MessageView.hpp"
#include "ui/PresentationView.hpp"
#include "ui/SoundLibrary.hpp"
#include "ui/TextSession.hpp"
#include "ui/UiLua.hpp"
#include "ui/components/BuiltInComponents.hpp"

namespace haylen::plugins {

UiPlugin::UiPlugin() {
    ui::BuiltInComponents::registerAll(components);

    for (ui::Theme theme : {ui::Theme::dark(), ui::Theme::light(), ui::Theme::darkHighContrast(), ui::Theme::lightHighContrast()}) {
        const std::string name = theme.getName();
        themes.emplace(name, std::move(theme));
    }
}

UiPlugin::~UiPlugin() = default;

void UiPlugin::start(core::Engine& engine) {
    owner = &engine;
    backend = std::make_unique<ui::Backend>(engine.getGraphics(), engine.getWindow(), engine.getSystem(), core::EmbeddedFiles::getDefaultFont(), core::EmbeddedFiles::getMonospaceFont());
    safeAreaVisible = engine.getConfig().debug.showSafeArea;
    scaling = engine.getConfig().uiScaling;
    // clang-format off
    images = std::make_unique<ui::ImageLibrary>(engine.getAssets(), engine.getGraphics(), engine.getJobs());
    sounds = std::make_unique<ui::SoundLibrary>(engine.getAssets(), engine.getAudio());
    ui::Context::Sources sources{
        .images = [this](std::string_view path, float scale) { return images->getTexture(path, getTheme().getImageFilter(), scale); },
        .imageSizes = [this](std::string_view path) { return images->getSize(path, getTheme().getImageFilter()); },
        .fonts = [this](std::string_view name) { return getFontFamily(*owner, name); },
        .themes = [this](std::string_view name) { return findTheme(name); },
        .textures = [this](std::string_view path, graphics::Texture::Options options) { return images->getTexture(path, options.filter, 1.0F); },
        .sounds = [this](ui::Theme::Sound role, const ui::Theme::Cue& cue) { sounds->play(role, cue, elapsed); },
        .soundFiles = [this](std::string_view path) { sounds->load(path); },
    };
    context = std::make_unique<ui::Context>(*backend, focus, engine.getPlugin<LocalizationPlugin>().getCatalog(), engine.getInput(), std::move(sources), engine.getPlugin<TextPlugin>().getRegistry());
    // clang-format on

    for (const auto& [name, theme] : themes) {
        sounds->load(theme);
    }

    setTheme(themeName);
    navigation = std::make_unique<ui::Navigation>(engine, *this);
    presentationView = std::make_unique<ui::PresentationView>(*backend, *context);
    messageView = std::make_unique<ui::MessageView>(*context, engine.getAccessibility());
    // clang-format off
    focus.setBackHandler([this] {
        if (!navigation->canGoBack()) {
            return false;
        }

        backRequested = true;
        return true;
    });
    // clang-format on
}

void UiPlugin::stop(core::Engine& engine) {
    if (context) {
        context->releaseControls(engine.getVirtualInput());
    }

    engine.getInput().setPointerCaptured(false);
    engine.getActionMaps().setCapture({});

    // Pending image and sound loads finish into libraries that no longer exist, so they drop their result.
    images.reset();
    sounds.reset();
    fontPaths.clear();
    themeFiles.clear();
    fontFamilies.clear();

    // No GUI is left to report events, and listeners holding Lua functions let go before the Lua state closes.
    if (navigation) {
        navigation->closeAll();
        navigation.reset();
    }

    focus.setBackHandler({});
    backRequested = false;
    presentationView.reset();
    messageView.reset();
    for (const Mounted& entry : guis) {
        (void)entry.gui->detach();
    }

    guis.clear();
    events.clear();
    context.reset();
    backend.reset();
    engine.getAppTextField().setTakenOver(false);
    owner = nullptr;
}

ui::Backend& UiPlugin::getBackend() {
    if (!backend) {
        throw std::logic_error("The UI plugin has not started.");
    }

    return *backend;
}

ui::Navigation& UiPlugin::getNavigation() {
    if (!navigation) {
        throw std::logic_error("The UI plugin has not started.");
    }

    return *navigation;
}

ui::ImageLibrary& UiPlugin::getImages() {
    return *images;
}

ui::Context& UiPlugin::getContext() {
    if (!context) {
        throw std::logic_error("The UI plugin has not started.");
    }

    return *context;
}

void UiPlugin::event(core::Engine& engine, const platform::Event& event) {
    if (backend) {
        backend->handleEvent(event, engine.getViewport());
    }
}

void UiPlugin::beginFrame(core::Engine& engine, float) {
    // The UI runs a frame of ImGui only when the frame draws, since ImGui lets go of the windows that a frame leaves out, and with them the focus that the remote of a TV moves on from, such as while native UI covers the app.
    if (!engine.isDrawing()) {
        return;
    }

    // The UI keeps real time, so menus still animate while the time scale pauses the world.
    const auto delta = static_cast<float>(engine.getClock().getUnscaledDelta());
    elapsed += delta;
    drawBegun = false;
    guiAppeared = false;
    navigationInput.update(engine.getActions(), engine.getInput(), engine.getVirtualInput(), engine.isHalted() || engine.getScenes().isInputBlocked(), focus.getOwner() == ui::FocusNavigator::Owner::PlayArea);
    const graphics::Viewport& viewport = engine.getViewport();
    const math::Vec2 pixelsPerUnit = viewport.getPixelsPerUnit();
    backend->setScale(scaling.resolve(std::min(pixelsPerUnit.x, pixelsPerUnit.y), engine.getWindow().getDpiScale(), viewport.getVisibleRect().getSize()));

    // The UI follows the accessibility settings of the system: its theme switches with the contrast and the transparency, and its text scales with the system unless the app set a scale.
    const platform::AccessibilitySettings& settings = engine.getAccessibility().getSettings();
    if (settings.highContrast != shownHighContrast || settings.reducedTransparency != shownOpaque) {
        applyTheme(settings.highContrast, settings.reducedTransparency);
    }

    context->setTextScale(textScale.value_or(settings.textScale));
    backend->beginFrame(delta, viewport, engine.getInput(), navigationInput);

    if (backend->getPointsPerUnit() != styledPoints) {
        styledPoints = backend->getPointsPerUnit();
        getTheme().applyTo(ImGui::GetStyle(), styledPoints);
    }

    context->beginFrame(elapsed, delta);
    images->beginFrame();

    // Text reads in the current language, and an automatic direction takes the one that language declares.
    const localization::Catalog& catalog = engine.getPlugin<LocalizationPlugin>().getCatalog();
    const bool known = !catalog.getLanguage().empty();
    const text::Direction resolved = direction == text::Direction::Auto ? (known ? catalog.getDirection(catalog.getLanguage()) : text::Direction::LeftToRight) : direction;
    context->setBaseWriting(resolved, catalog.getLanguage());
    focus.update(navigationInput, engine.getInput().getLastDevice(), engine.getWindow().hasPointerDevice());

    // The actions of screen readers reach the elements of the last frame, which the tree forgets once it starts the elements of this one.
    platform::Accessibility& accessibility = engine.getAccessibility();
    ui::AccessibilityTree& tree = context->getAccessibility();
    accessibility.takeRequests(accessibilityRequests);

    for (const platform::Accessibility::Request& request : accessibilityRequests) {
        perform(request);
    }

    tree.setEnabled(accessibility.isTreeWanted());
    tree.beginFrame();
}

// A button that takes no focus hears its press while it draws, and the actions on the other elements reach their focus targets.
void UiPlugin::perform(const platform::Accessibility::Request& request) {
    using Element = platform::AccessibilityElement;
    ui::AccessibilityTree& tree = context->getAccessibility();
    if (request.action == Element::Activate && tree.press(request.element)) {
        return;
    }

    const std::optional<ImGuiID> target = tree.findTarget(request.element);
    if (!target) {
        return;
    }

    const ui::FocusDirection increase = context->isRightToLeft() ? ui::FocusDirection::Left : ui::FocusDirection::Right;
    const ui::FocusDirection decrease = context->isRightToLeft() ? ui::FocusDirection::Right : ui::FocusDirection::Left;
    switch (request.action) {
    case Element::Focus:
        (void)focus.focusTarget(*target);
        break;
    case Element::Activate:
        (void)focus.pressTarget(*target);
        break;
    case Element::Increment:
        (void)focus.stepTarget(*target, increase);
        break;
    case Element::Decrement:
        (void)focus.stepTarget(*target, decrease);
        break;
    }
}

// The tree goes to the platform in framebuffer pixels, which screen readers outline on the screen.
void UiPlugin::publishAccessibility(core::Engine& engine) {
    ui::AccessibilityTree& tree = context->getAccessibility();
    if (!tree.isEnabled()) {
        return;
    }

    tree.endFrame(backend->getImGuiContext()->NavId);
    const graphics::Viewport& viewport = engine.getViewport();
    for (platform::AccessibilityElement& element : tree.getElements()) {
        const math::Vec2 min = viewport.toFramebuffer(backend->toDesign(element.bounds.getMin()));
        const math::Vec2 max = viewport.toFramebuffer(backend->toDesign(element.bounds.getMax()));
        element.bounds = math::Rect::fromMinMax(min, max);
    }

    engine.getAccessibility().publish(tree.getElements(), tree.getFocused());
}

// The navigation moves on with the real time of the UI, after the events of the GUIs, so a screen that a handler pushes starts its transition in this frame.
void UiPlugin::update(core::Engine& engine, float) {
    deliverEvents(engine);
    if (std::exchange(backRequested, false)) {
        navigation->goBack();
    }

    const ui::PresentationView::Requests requests = presentationView->takeRequests();
    if (const std::shared_ptr<ui::Presentation> tapped = requests.tapped.lock()) {
        static_cast<void>(tapped->dismissByPerson());
    }

    if (const std::shared_ptr<ui::Presentation> released = requests.released.lock()) {
        released->release();
    }

    if (const std::shared_ptr<ui::Presentation> cycled = requests.cycled.lock(); cycled && !cycled->isDismissed()) {
        cycled->setDetent((cycled->getDetent() + 1) % cycled->getOptions().detents.size());
    }

    navigation->update(static_cast<float>(engine.getClock().getUnscaledDelta()));
}

void UiPlugin::render(core::Engine&) {
    navigation->render();
}

void UiPlugin::deliverEvents(core::Engine& engine) {
    // Handlers may mount or unmount GUIs, so the loop walks a copy and skips GUIs that are gone.
    delivering.assign(guis.begin(), guis.end());
    for (const Mounted& entry : delivering) {
        ui::Gui& gui = *entry.gui;
        for (const ui::Event& event : gui.takeEvents()) {
            if (!isMounted(gui)) {
                break;
            }

            deliver(engine, gui, event);
        }
    }

    delivering.clear();
}

// Listeners hear an event before the handlers of its node.
void UiPlugin::deliver(core::Engine& engine, ui::Gui& gui, const ui::Event& event) {
    events.emit(gui, event);
    ui::UiLua::deliverEvent(engine.getLuaState(), gui, event);
}

// An event delivered at once runs inside whatever changed the GUI, so a failing handler goes to the error screen instead of stopping that change halfway.
void UiPlugin::deliverAtOnce(ui::Gui& gui, const ui::Event& event) {
    core::Engine& engine = *owner;
    lua::Runtime::runReporting(engine.getLuaState(), [&] { deliver(engine, gui, event); });
}

// Every view draws the GUIs of its scenes, a view that leaves through a transition into its own image before the frame ends, and the current view also the GUIs that belong to no scene, which ends the frame. During an effect that shows both scenes, such as a slide, the GUIs of no scene wait for the view of the effect, which draws them on the screen above it, so the bars of an app stay in place while its pages change.
void UiPlugin::renderUi(core::Engine& engine, const core::SceneView& view) {
    ui::Backend& drawing = getBackend();
    if (view.effect && !view.current) {
        drawing.renderDeferred(engine.getRenderer2D());
        return;
    }
    if (!drawing.isFrameActive()) {
        return;
    }

    if (!drawBegun) {
        drawBegun = true;
        shownGuis.clear();
        waitingGuis.clear();
        prepareGuis();
        focus.beginDraw();
    }

    if (view.current) {
        drawCurrent(engine, view);
    } else if (!view.scenes.empty()) {
        drawLeaving(engine, view);
    }
}

// Collects the GUIs a view draws in layer order, into storage every frame reuses: those of its scenes, those of no scene, or both. A scene in the current view and in a leaving view, such as one under a transparent scene pushed through an effect that shows both, keeps its GUIs in the current view. A GUI of a region draws right after the GUI of its region, which lays out the area first.
void UiPlugin::collectGuis(core::Engine& engine, const core::SceneView& view, bool sceneGuis, bool freeGuis) {
    const std::vector<core::SceneView>& views = engine.getScenes().getViews();
    const auto current = std::ranges::find_if(views, &core::SceneView::current);
    // clang-format off
    const auto shows = [](const core::SceneView& candidate, const core::Scene* scene) {
        return std::ranges::any_of(candidate.scenes, [scene](const std::shared_ptr<core::Scene>& shown) { return shown.get() == scene; });
    };
    // clang-format on

    drawnGuis.clear();
    for (const Mounted& entry : guis) {
        if (!entry.scened) {
            if (freeGuis) {
                drawnGuis.push_back(&entry);
            }

            continue;
        }

        const std::shared_ptr<const core::Scene> scene = entry.scene.lock();
        if (sceneGuis && scene && shows(view, scene.get()) && (view.current || current == views.end() || !shows(*current, scene.get()))) {
            drawnGuis.push_back(&entry);
        }
    }

    // clang-format off
    std::ranges::sort(drawnGuis, [](const Mounted* lhs, const Mounted* rhs) {
        const bool lhsRegion = lhs->gui->getPlacement() == ui::Placement::Region;
        const bool rhsRegion = rhs->gui->getPlacement() == ui::Placement::Region;
        return std::tie(lhs->drawLayer, lhs->drawOrder, lhsRegion, lhs->order) < std::tie(rhs->drawLayer, rhs->drawOrder, rhsRegion, rhs->order);
    });
    // clang-format on
}

// The area of a GUI of a region is where the node of its region drew in this frame or the last one, so it follows the GUI of its region in the frame that GUI changes, and it has none while that GUI is gone, hidden or no longer draws the node.
std::optional<math::Rect> UiPlugin::findArea(const Mounted& entry) const {
    const ui::Backend& drawing = *backend;
    if (entry.gui->getPlacement() == ui::Placement::Safe) {
        return drawing.getLiftedSafeRect();
    }

    if (entry.gui->getPlacement() == ui::Placement::Screen) {
        return drawing.getLiftedDisplayRect();
    }

    const std::shared_ptr<ui::Gui> host = entry.host.lock();
    if (!host || !host->isVisible() || !isMounted(*host)) {
        return std::nullopt;
    }

    const ui::Component* node = host->find(entry.hostNode);
    if (node == nullptr || node->getDrawnFrame() + 1 < context->getFrame() || node->getBounds().isEmpty()) {
        return std::nullopt;
    }

    return node->getBounds();
}

// Collections bind their cells and run their binders before anything draws, where Lua may run. A binder may unmount other GUIs, so the pass walks a copy and skips GUIs that are gone.
void UiPlugin::prepareGuis() {
    preparing.clear();
    for (const Mounted& entry : guis) {
        preparing.push_back(entry.gui);
    }

    for (const std::shared_ptr<ui::Gui>& gui : preparing) {
        if (gui->isVisible() && isMounted(*gui)) {
            gui->prepare(*context);
        }
    }

    preparing.clear();
}

void UiPlugin::beginWindow(const char* name, ImGuiWindowFlags flags) {
    ui::Backend& drawing = getBackend();
    const math::Rect display = drawing.getDisplayRect();
    ImGui::SetNextWindowPos({0.0F, 0.0F});
    ImGui::SetNextWindowSize(ui::ImGuiConverter::toImVec2(display.getSize()));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0F, 0.0F});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
    const ImGuiWindowFlags common = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin(name, nullptr, common | flags);
    ImGui::PopStyleVar(2);
}

// Every GUI moves up together while the on-screen keyboard would cover the focused text field. The GUIs share one window, so the backend gives the pointer to the GUI that drew last over it.
void UiPlugin::drawGuis(std::size_t first, std::size_t last, bool inert) {
    for (std::size_t index = first; index < last; ++index) {
        const Mounted& entry = *drawnGuis[index];
        backend->beginGuiPointer(entry.gui.get());
        drawGui(entry, inert);
        backend->endGuiPointer();
    }
}

// The GUIs that collect before the GUI of the topmost presentation lie under it, or none while no presentation shows.
std::size_t UiPlugin::countUnderPresentation() const {
    const ui::Presentation* top = navigation->getTopModal();
    if (top == nullptr) {
        return 0;
    }

    const auto found = std::ranges::find(drawnGuis, top->getScreen()->getGui().get(), [](const Mounted* entry) { return entry->gui.get(); });
    return static_cast<std::size_t>(std::distance(drawnGuis.begin(), found));
}

// The GUI of a screen draws where the look of its screen places it, cut to its area while it moves, and without input while its transition plays. The GUI of a presentation draws over its backdrop and surface, and a sheet that is not modal draws in a window of its own above the GUIs of the app, so it takes the pointer over its surface while they keep theirs.
void UiPlugin::drawGui(const Mounted& entry, bool inert) {
    ui::Navigation::Look look{.shown = true};
    std::shared_ptr<ui::Presentation> presentation;
    if (entry.screen != nullptr) {
        look = navigation->getLook(*entry.screen);
        presentation = entry.screen->getPresentation();
    }

    if (!look.shown) {
        return;
    }

    if (presentation) {
        drawPresented(entry, *presentation, look, inert);
        return;
    }

    const std::optional<math::Rect> area = findArea(entry);
    if (!area) {
        return;
    }

    const math::Rect display = backend->getLiftedDisplayRect();
    const math::Vec2 offset = getLookOffset(look, *area, display.getBottom() - area->y);
    drawScreenGui(entry, look, *area, offset, offset != math::Vec2{} ? std::optional(*area) : std::nullopt, inert);
}

void UiPlugin::drawPresented(const Mounted& entry, ui::Presentation& presentation, const ui::Navigation::Look& look, bool inert) {
    const ui::Presentation::Frame frame = presentationView->prepare(presentation, *entry.gui);
    const ui::Presentation::Layout layout = presentation.getLayout(frame);
    const math::Vec2 offset = presentationView->getOffset(layout, look);
    const bool detached = !presentation.isModal() && !inert;
    if (detached) {
        const math::Rect surface = layout.surface.translated(offset);
        std::array<char, 40> name{};
        std::snprintf(name.data(), name.size(), "##haylen-sheet%p", static_cast<const void*>(&presentation));
        ImGui::SetNextWindowPos(ui::ImGuiConverter::toImVec2(surface.getMin()));
        ImGui::SetNextWindowSize(ui::ImGuiConverter::toImVec2(surface.getSize()));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0F, 0.0F});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::Begin(name.data(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs);
        ImGui::PopStyleVar(2);
    }

    presentationView->drawChrome(presentation, layout, look, offset, !inert);
    drawScreenGui(entry, look, layout.area, offset, frame.display, inert);
    if (detached) {
        ImGui::End();
    }
}

// A GUI that moves draws cut to its clip, its area for a screen of a stack and the display for a presentation.
void UiPlugin::drawScreenGui(const Mounted& entry, const ui::Navigation::Look& look, const math::Rect& area, math::Vec2 offset, const std::optional<math::Rect>& clip, bool inert) {
    if (entry.gui->getRoot().getDrawnFrame() == 0) {
        guiAppeared = true;
    }

    if (clip) {
        ImGui::PushClipRect(ui::ImGuiConverter::toImVec2(clip->getMin()), ui::ImGuiConverter::toImVec2(clip->getMax()), true);
    }

    const bool still = inert || look.inert;
    const bool suspended = focus.areTargetsSuspended();
    if (still) {
        backend->beginInert();
        focus.suspendTargets(true);
    }

    const float opacity = look.effect == ui::NavigationTransition::Effect::Fade ? 1.0F - look.distance : 1.0F;
    const std::uint64_t waitsBefore = images->getWaits();
    ImGui::PushID(entry.gui.get());
    entry.gui->draw(*context, area.translated(offset), opacity);
    ImGui::PopID();
    if (images->getWaits() != waitsBefore) {
        waitingGuis.push_back(entry.gui.get());
    }

    if (still) {
        focus.suspendTargets(suspended);
        backend->endInert();
    }
    if (clip) {
        ImGui::PopClipRect();
    }

    shownGuis.push_back(entry.gui.get());
}

bool UiPlugin::isComplete(const ui::Gui& gui) const {
    return gui.getRoot().getDrawnFrame() != 0 && std::ranges::find(waitingGuis, &gui) == waitingGuis.end();
}

// A slide moves the screen across the width of its area, mirrored in a right to left interface, and a rise moves it down by the room under the top of what rises.
math::Vec2 UiPlugin::getLookOffset(const ui::Navigation::Look& look, const math::Rect& area, float riseRoom) const {
    switch (look.effect) {
    case ui::NavigationTransition::Effect::Slide: {
        const float mirror = context->isRightToLeft() ? -1.0F : 1.0F;
        return {look.distance * look.side * mirror * area.width, 0.0F};
    }
    case ui::NavigationTransition::Effect::Rise:
        return {0.0F, look.distance * riseRoom};
    case ui::NavigationTransition::Effect::None:
    case ui::NavigationTransition::Effect::Fade:
        break;
    }

    return {};
}

// The GUIs of a leaving view take no input and no focus, and they draw into the image of their view right away.
void UiPlugin::drawLeaving(core::Engine& engine, const core::SceneView& view) {
    beginWindow("##haylen-leaving", ImGuiWindowFlags_NoInputs);
    focus.suspendTargets(true);
    collectGuis(engine, view, true, false);
    drawGuis(0, drawnGuis.size(), false);
    focus.suspendTargets(false);
    const ImGuiWindow& window = *ImGui::GetCurrentWindow();
    ImGui::End();
    getBackend().renderWindow(engine.getRenderer2D(), window);
}

// The GUIs of no scene take no input and no focus during an effect that shows both scenes. They draw first, so the GUIs of their regions lay out in the areas of this frame, but they reach the screen only once the effect drew.
void UiPlugin::drawAbove(core::Engine& engine, const core::SceneView& view) {
    ui::Backend& drawing = getBackend();
    drawing.beginDeferring();
    beginWindow("##haylen-above", ImGuiWindowFlags_NoInputs);
    focus.suspendTargets(true);
    collectGuis(engine, view, false, true);
    drawGuis(0, drawnGuis.size(), false);
    focus.suspendTargets(false);
    ImGui::End();
    drawing.endDeferring();
}

// The messages keep inside the area of the app while its node draws and no modal presentation covers it, and lie in the safe area otherwise.
math::Rect UiPlugin::getMessageArea() const {
    const math::Rect safe = backend->getLiftedSafeRect();
    const std::optional<ui::Region>& area = navigation->getMessageArea();
    if (!area || navigation->getTopModal() != nullptr) {
        return safe;
    }

    const ui::Component* node = area->gui->find(area->node);
    if (node == nullptr || node->getDrawnFrame() + 1 < context->getFrame() || node->getBounds().isEmpty()) {
        return safe;
    }

    return node->getBounds().intersection(safe);
}

// The GUIs under the topmost modal presentation draw in a window of their own that takes no input, with child windows that never take the pointer, and reach the screen before the presentation draws over them, without elements for screen readers. They shrink about the middle of the screen while a sheet near the top of a compact screen covers them.
void UiPlugin::drawUnder(core::Engine& engine, std::size_t count) {
    beginWindow("##haylen-under", ImGuiWindowFlags_NoInputs);
    const ui::Presentation& top = *navigation->getTopModal();
    const float scale = presentationView->getBehindScale(top, navigation->getLook(*top.getScreen()));
    const ui::Transform shrunk{.scale = {scale, scale}};
    const math::Vec2 middle = backend->getLiftedDisplayRect().getCenter();
    const int firstVertex = ImGui::GetCurrentWindow()->DrawList->VtxBuffer.Size;
    const int firstWindow = GImGui->WindowsActiveCount;
    if (shrunk.isReshaping()) {
        context->pushReshape(shrunk, middle);
    }

    ui::AccessibilityTree& tree = context->getAccessibility();
    tree.beginHidden();
    drawGuis(0, count, true);
    tree.endHidden();
    if (shrunk.isReshaping()) {
        context->popReshape();
        ui::Component::reshapeSince(firstVertex, firstWindow, shrunk, middle);
    }

    const ImGuiWindow& window = *ImGui::GetCurrentWindow();
    ImGui::End();
    getBackend().renderWindow(engine.getRenderer2D(), window);
}

void UiPlugin::drawCurrent(core::Engine& engine, const core::SceneView& view) {
    ui::Backend& drawing = getBackend();
    const bool aboveTransition = view.target.isValid() && engine.getScenes().isShowingBothScenes();
    if (aboveTransition) {
        drawAbove(engine, view);
    }

    collectGuis(engine, view, true, !aboveTransition);
    const std::size_t under = countUnderPresentation();
    if (under > 0) {
        drawUnder(engine, under);
    }

    beginWindow("##haylen-guis", ImGuiWindowFlags_NoNavInputs);
    drawing.setTransparentWindow();
    drawGuis(under, drawnGuis.size(), false);
    messageView->draw(navigation->getMessages(), getMessageArea());

    // The GUIs that no view shows, such as those of a covered scene, hear that they stopped drawing.
    for (const Mounted& entry : guis) {
        if (std::ranges::find(shownGuis, entry.gui.get()) == shownGuis.end()) {
            entry.gui->skip(*context);
        }
    }

    ImGui::End();
    focus.endDraw();
    publishAccessibility(engine);

    if (safeAreaVisible) {
        drawSafeArea();
    }

    drawing.render(engine.getRenderer2D());
    engine.getAppTextField().setTakenOver(drawing.getTextSession().isActive());
    context->applyControls(engine.getVirtualInput());

    // The action map of the next frame ignores mouse buttons while the interface owns the pointer, so a click on a button never reaches gameplay, and it leaves the presses the interface answers itself to the interface, such as the cancel that closes a popup.
    engine.getInput().setPointerCaptured(isUsingPointer());
    engine.getActionMaps().setCapture(getCapture());
}

// A text field that edits takes every key, and a control that listens for a binding takes every key and gamepad button. A control with the focus takes every navigation action and Tab, a play area with the focus leaves the directions, accept and menu to the game, and the UI otherwise takes the presses it answers itself.
input::ActionMap::Capture UiPlugin::getCapture() const {
    using Action = ui::NavigationInput::Action;
    const ImGuiContext& state = *backend->getImGuiContext();
    const bool listening = focus.isEditing() && state.ActiveIdUsingAllKeyboardKeys;
    input::ActionMap::Capture capture{.keyboard = listening || state.PlatformImeData.WantTextInput};
    if (listening) {
        capture.buttons.set();
    }

    const ui::FocusNavigator::Owner holder = focus.getOwner();
    const bool control = holder == ui::FocusNavigator::Owner::Control;

    if (holder != ui::FocusNavigator::Owner::None) {
        capture.keys.set(static_cast<std::size_t>(input::Key::Tab));
    }

    const std::array<std::pair<Action, bool>, ui::NavigationInput::kActionCount> answers{{
        {Action::Accept, focus.answersAccept()},
        {Action::Cancel, focus.answersCancel() || navigation->canGoBack()},
        {Action::Left, control},
        {Action::Right, control},
        {Action::Up, control},
        {Action::Down, control},
        {Action::Menu, control},
        {Action::Focus, focus.answersFocus()},
        {Action::PagePrevious, control},
        {Action::PageNext, control},
        {Action::First, control},
        {Action::Last, control},
    }};

    for (const auto& [action, answered] : answers) {
        if (answered) {
            addCapture(capture, navigationInput.getBindings(action));
        }
    }

    return capture;
}

void UiPlugin::addCapture(input::ActionMap::Capture& capture, const std::vector<input::ActionMap::Binding>& bindings) {
    for (const input::ActionMap::Binding& binding : bindings) {
        switch (binding.source) {
        case input::ActionMap::Binding::Source::Key:
            capture.keys.set(static_cast<std::size_t>(binding.key));
            break;
        case input::ActionMap::Binding::Source::GamepadButton:
            capture.buttons.set(static_cast<std::size_t>(binding.gamepadButton));
            break;
        case input::ActionMap::Binding::Source::GamepadAxis:
            capture.axes.set(static_cast<std::size_t>(binding.gamepadAxis));
            break;
        case input::ActionMap::Binding::Source::GamepadStick:
            capture.axes.set(static_cast<std::size_t>(binding.rightStick ? input::GamepadAxis::RightX : input::GamepadAxis::LeftX));
            capture.axes.set(static_cast<std::size_t>(binding.rightStick ? input::GamepadAxis::RightY : input::GamepadAxis::LeftY));
            break;
        default:
            break;
        }
    }
}

void UiPlugin::drawSafeArea() {
    const math::Rect display = backend->getDisplayRect();
    const math::Rect safe = backend->getSafeRect();
    const ImU32 shade = ui::ImGuiConverter::toImU32(context->getColor(ui::Theme::Color::Danger).withAlpha(0.28F));
    const ImU32 line = ui::ImGuiConverter::toImU32(context->getColor(ui::Theme::Color::Warning));
    ImDrawList& list = *ImGui::GetForegroundDrawList();
    list.AddRectFilled({display.x, display.y}, {display.getRight(), safe.y}, shade);
    list.AddRectFilled({display.x, safe.getBottom()}, {display.getRight(), display.getBottom()}, shade);
    list.AddRectFilled({display.x, safe.y}, {safe.x, safe.getBottom()}, shade);
    list.AddRectFilled({safe.getRight(), safe.y}, {display.getRight(), safe.getBottom()}, shade);
    list.AddRect({safe.x, safe.y}, {safe.getRight(), safe.getBottom()}, line, 0.0F, 3.0F);
    drawSafeAreaLabel(list, display, safe, line);
}

// The insets read in the widest band outside the safe area that holds them, on one line in the top or bottom band or a line per edge in a side band, so the label never covers the interface. Without such a band they read on a dark pill at the bottom of the screen.
void UiPlugin::drawSafeAreaLabel(ImDrawList& list, const math::Rect& display, const math::Rect& safe, ImU32 color) {
    const float top = safe.y - display.y;
    const float right = display.getRight() - safe.getRight();
    const float bottom = display.getBottom() - safe.getBottom();
    const float left = safe.x - display.x;
    ImFont* font = ImGui::GetFont();
    const float size = kSafeAreaLabelSize;
    const float padding = size * 0.5F;
    const std::string line = std::format("Safe area: top {:.0f}, right {:.0f}, bottom {:.0f}, left {:.0f}", top, right, bottom, left);
    const ImVec2 lineSize = font->CalcTextSizeA(size, std::numeric_limits<float>::max(), 0.0F, line.c_str());
    if (std::max(top, bottom) >= lineSize.y + padding) {
        const float y = bottom >= lineSize.y + padding ? safe.getBottom() + (bottom - lineSize.y) * 0.5F : display.y + (top - lineSize.y) * 0.5F;
        list.AddText(font, size, {std::floor(display.getCenter().x - lineSize.x * 0.5F), std::floor(y)}, color, line.c_str());
        return;
    }

    const std::string lines = std::format("Safe area\ntop {:.0f}\nright {:.0f}\nbottom {:.0f}\nleft {:.0f}", top, right, bottom, left);
    const ImVec2 linesSize = font->CalcTextSizeA(size, std::numeric_limits<float>::max(), 0.0F, lines.c_str());
    if (std::max(left, right) >= linesSize.x + padding * 2.0F) {
        const float x = left >= linesSize.x + padding * 2.0F ? display.x + (left - linesSize.x) * 0.5F : safe.getRight() + (right - linesSize.x) * 0.5F;
        list.AddText(font, size, {std::floor(x), std::floor(safe.getCenter().y - linesSize.y * 0.5F)}, color, lines.c_str());
        return;
    }

    const ImVec2 corner{std::floor(display.getCenter().x - lineSize.x * 0.5F), std::floor(display.getBottom() - lineSize.y - padding * 3.0F)};
    list.AddRectFilled({corner.x - padding, corner.y - padding * 0.5F}, {corner.x + lineSize.x + padding, corner.y + lineSize.y + padding * 0.5F}, IM_COL32(0, 0, 0, 200), lineSize.y);
    list.AddText(font, size, corner, color, line.c_str());
}

std::shared_ptr<ui::Gui> UiPlugin::createGui(const core::Json& tree, ui::Placement placement) const {
    return std::make_shared<ui::Gui>(components, tree, placement);
}

void UiPlugin::mount(std::shared_ptr<ui::Gui> gui, int layer, const std::shared_ptr<const core::Scene>& scene) {
    if (gui && gui->getPlacement() == ui::Placement::Region) {
        throw std::invalid_argument("Only the navigation mounts a GUI of the region placement, as the GUI of a screen.");
    }

    add({.gui = std::move(gui), .scene = scene, .scened = scene != nullptr, .layer = layer});
}

// The GUI of a screen of a stack belongs to the scene of the GUI of its area and draws right after that GUI, and the GUI of a presentation belongs to no scene and draws above every other GUI.
void UiPlugin::mountScreen(const ui::Screen& screen, const ui::Region* region) {
    if (region == nullptr) {
        add({.gui = screen.getGui(), .screen = &screen});
        return;
    }

    const Mounted& host = findRegionHost(*region);
    add({.gui = screen.getGui(), .scene = host.scene, .scened = host.scened, .screen = &screen, .host = region->gui, .hostNode = region->node, .drawLayer = host.drawLayer, .drawOrder = host.drawOrder});
}

void UiPlugin::moveScreen(const ui::Gui& gui, const ui::Region& region) {
    const Mounted host = findRegionHost(region);
    const auto found = std::ranges::find_if(guis, [&](const Mounted& entry) { return entry.gui.get() == &gui; });
    if (found == guis.end()) {
        return;
    }

    found->scene = host.scene;
    found->scened = host.scened;
    found->host = region.gui;
    found->hostNode = region.node;
    found->drawLayer = host.drawLayer;
    found->drawOrder = host.drawOrder;
}

// A GUI of no region takes its own place in the drawing order, and the GUI of a presentation draws above every other GUI.
void UiPlugin::add(Mounted entry) {
    if (owner == nullptr) {
        throw std::logic_error("The UI plugin has not started.");
    }

    if (!entry.gui || isMounted(*entry.gui)) {
        throw std::invalid_argument("Only a GUI that is not mounted can be mounted.");
    }

    entry.order = nextOrder++;
    if (entry.hostNode.empty()) {
        entry.drawOrder = entry.order;
        entry.drawLayer = entry.screen != nullptr ? kPresentationLayer : entry.layer;
    }

    const std::shared_ptr<ui::Gui> gui = entry.gui;
    guis.push_back(std::move(entry));
    gui->attach([this](ui::Gui& target, const ui::Event& event) { deliverAtOnce(target, event); });
    owner->getEvents().emitWith(core::LifecycleEvent::kGuiMounted, gui);
}

// GUIs are only mounted while the plugin runs, so a GUI that is found has an engine to report to.
bool UiPlugin::unmount(const ui::Gui& gui) {
    const auto found = std::ranges::find_if(guis, [&](const Mounted& entry) { return entry.gui.get() == &gui; });
    if (found == guis.end()) {
        return false;
    }

    gui.checkNotPreparing();
    const std::shared_ptr<ui::Gui> unmounted = found->gui;
    guis.erase(found);
    focus.forget(*unmounted);

    for (const ui::Event& event : unmounted->detach()) {
        deliverAtOnce(*unmounted, event);
    }

    owner->getEvents().emitWith(core::LifecycleEvent::kGuiUnmounted, unmounted);
    ui::UiLua::forgetGui(owner->getLuaState(), gui);
    if (navigation) {
        navigation->closeHostedBy(*unmounted);
    }

    return true;
}

const UiPlugin::Mounted& UiPlugin::findRegionHost(const ui::Region& region) const {
    const auto found = std::ranges::find_if(guis, [&](const Mounted& entry) { return entry.gui == region.gui; });
    if (!region.gui || found == guis.end()) {
        throw std::invalid_argument("The GUI of a region must be mounted.");
    }
    if (region.gui->find(region.node) == nullptr) {
        throw std::invalid_argument(std::format("The GUI of the region has no node with the id \"{}\".", region.node));
    }
    return *found;
}

void UiPlugin::checkRegion(const ui::Region& region) const {
    static_cast<void>(findRegionHost(region));
}

bool UiPlugin::isMounted(const ui::Gui& gui) const {
    return std::ranges::any_of(guis, [&](const Mounted& entry) { return entry.gui.get() == &gui; });
}

std::shared_ptr<ui::Gui> UiPlugin::findMounted(const ui::Gui* gui) const {
    const auto found = std::ranges::find_if(guis, [&](const Mounted& entry) { return entry.gui.get() == gui; });
    return found != guis.end() ? found->gui : nullptr;
}

void UiPlugin::addTheme(ui::Theme theme) {
    const std::string name = theme.getName();
    const auto added = themes.insert_or_assign(name, std::move(theme)).first;
    if (sounds) {
        sounds->load(added->second);
    }

    if (backend && (name == themeName || name == themes.find(themeName)->second.getHighContrast())) {
        setTheme(themeName);
    }
}

void UiPlugin::setTheme(std::string_view name) {
    const auto found = themes.find(name);
    if (found == themes.end()) {
        throw std::invalid_argument("The UI has no theme named \"" + std::string(name) + "\".");
    }

    const std::string& variant = found->second.getHighContrast();
    if (!variant.empty() && !themes.contains(variant)) {
        throw std::invalid_argument("The theme \"" + std::string(name) + "\" names the high contrast theme \"" + variant + "\", which the UI does not know.");
    }

    themeName = found->first;
    applyTheme(shownHighContrast, shownOpaque);
}

void UiPlugin::applyTheme(bool highContrast, bool reducedTransparency) {
    shownHighContrast = highContrast;
    shownOpaque = reducedTransparency;
    const ui::Theme& chosen = themes.find(themeName)->second;
    const auto variant = highContrast && !chosen.getHighContrast().empty() ? themes.find(chosen.getHighContrast()) : themes.end();
    shownTheme = variant != themes.end() ? variant->second : chosen;

    // Higher contrast changes how the UI looks and never how it sounds, so the high contrast theme plays the sounds of the chosen theme.
    for (std::size_t index = 0; index < ui::Theme::kSoundCount; ++index) {
        const auto role = static_cast<ui::Theme::Sound>(index);
        const ui::Theme::Cue* cue = chosen.getSound(role);
        shownTheme.setSound(role, cue != nullptr ? std::optional<ui::Theme::Cue>(*cue) : std::nullopt);
    }

    if (reducedTransparency) {
        shownTheme.makeSurfacesOpaque();
    }

    if (!backend) {
        return;
    }

    const ui::Theme& theme = shownTheme;
    for (std::size_t index = 0; index < ui::Theme::kFontCount; ++index) {
        (void)backend->getFont(theme.getFont(static_cast<ui::Theme::Font>(index)).font);
    }

    backend->makeCurrent();
    styledPoints = backend->getPointsPerUnit();
    theme.applyTo(ImGui::GetStyle(), styledPoints);

    // The windows of ImGui draw in the body font at the size of its em square, which ImGui measures from the ascent to the descent of the face.
    const ui::Theme::FontStyle& body = theme.getFont(ui::Theme::Font::Body);
    ImGui::GetIO().FontDefault = backend->getFont(body.font);
    ImGui::GetStyle().FontSizeBase = backend->getFontHeight(body.font, body.size);
    context->setTheme(theme);
}

const ui::Theme* UiPlugin::findTheme(std::string_view name) const {
    const auto found = themes.find(name);
    return found != themes.end() ? &found->second : nullptr;
}

std::vector<std::string> UiPlugin::getThemes() const {
    std::vector<std::string> names;
    for (const auto& [name, theme] : themes) {
        names.push_back(name);
    }

    return names;
}

std::string UiPlugin::addTheme(core::Engine& engine, const core::Json& definition, std::string_view base) {
    const auto baseTheme = themes.find(base);
    if (baseTheme == themes.end()) {
        throw std::invalid_argument("The UI has no theme named \"" + std::string(base) + "\" to start from.");
    }

    // clang-format off
    ui::Theme theme = ui::Theme::fromJson(definition, baseTheme->second, [&engine](std::string_view texture, graphics::Texture::Options options) {
        return engine.getAssets().texture(texture, options);
    });
    // clang-format on
    for (const auto& [name, file] : theme.getFontFiles()) {
        if (!getBackend().hasFont(name)) {
            addFont(engine, name, file);
        }
    }

    for (std::size_t index = 0; index < ui::Theme::kFontCount; ++index) {
        const std::string& font = theme.getFont(static_cast<ui::Theme::Font>(index)).font;
        if (!getBackend().hasFont(font)) {
            throw std::invalid_argument("The theme \"" + theme.getName() + "\" uses the font \"" + font + "\", which is neither registered nor listed in \"fontFiles\".");
        }
    }

    for (std::size_t index = 0; index < ui::Theme::kSoundCount; ++index) {
        const ui::Theme::Cue* cue = theme.getSound(static_cast<ui::Theme::Sound>(index));
        if (cue != nullptr && !engine.getAssets().exists(cue->sound)) {
            throw std::invalid_argument("The theme \"" + theme.getName() + "\" plays the sound file \"" + cue->sound + "\", which the package does not hold.");
        }
    }

    std::string name = theme.getName();
    addTheme(std::move(theme));
    return name;
}

std::string UiPlugin::loadTheme(core::Engine& engine, std::string_view path, std::string_view base) {
    std::string name = addTheme(engine, engine.getAssets().json(path), base);
    themeFiles.insert_or_assign(io::Path::normalize(path), std::string(base));
    return name;
}

// An editor may still be writing a file when it is seen, so a theme that fails to build waits for the next save.
void UiPlugin::packageChanged(core::Engine& engine, std::span<const std::string> paths) {
    for (const std::string& path : paths) {
        if (!io::Path::isInside(path, io::Path::kContentDirectory)) {
            continue;
        }

        const std::string_view asset = std::string_view(path).substr(io::Path::kContentDirectory.size() + 1);
        sounds->reload(asset);
        const auto theme = themeFiles.find(asset);
        if (theme == themeFiles.end() || !engine.getPackage().exists(path)) {
            continue;
        }

        try {
            const std::string name = addTheme(engine, engine.getAssets().json(theme->first), theme->second);
            core::Log::info("Reloaded the theme \"{}\" from \"{}\".", name, theme->first);
        } catch (const std::exception& error) {
            core::Log::warning("The theme \"{}\" could not be reloaded yet: {}", theme->first, error.what());
        }
    }
}

void UiPlugin::addFont(core::Engine& engine, const std::string& name, std::string_view path) {
    getBackend().addFont(name, {.regular = engine.getAssets().bytes(path)});
    fontPaths.insert_or_assign(name, std::string(path));
}

std::vector<std::uint8_t> UiPlugin::getTrueTypeData(const std::shared_ptr<text::Font>& face) {
    const auto* font = dynamic_cast<const text::TrueTypeFont*>(face.get());
    return font != nullptr ? std::vector<std::uint8_t>(font->getData().begin(), font->getData().end()) : std::vector<std::uint8_t>();
}

void UiPlugin::addFontFamily(const std::string& name, std::shared_ptr<text::FontFamily> family) {
    if (!family) {
        throw std::invalid_argument("The UI font \"" + name + "\" needs a family.");
    }

    if (fontFamilies.contains(name)) {
        throw std::invalid_argument("The UI already has a font named \"" + name + "\".");
    }

    // ImGui draws the TrueType faces of the family, each with the TrueType fallbacks, while rich text draws every face and fallback it has.
    const text::FontFamily::Faces& faces = family->getFaces();
    ui::Backend::FontFiles files{.regular = getTrueTypeData(faces.regular), .bold = getTrueTypeData(faces.bold), .italic = getTrueTypeData(faces.italic), .boldItalic = getTrueTypeData(faces.boldItalic)};
    if (!files.regular.empty()) {
        for (const std::shared_ptr<text::Font>& fallback : faces.fallbacks) {
            if (std::vector<std::uint8_t> data = getTrueTypeData(fallback); !data.empty()) {
                files.fallbacks.push_back(std::move(data));
            }
        }

        getBackend().addFont(name, std::move(files));
    }

    fontFamilies.emplace(name, std::move(family));
}

// A font registered from a file becomes a family with that font as its regular face the first time text asks for it, and the built-in monospace font is the mono face of the default family of the engine. Each draws what it lacks with the fonts of the system.
std::shared_ptr<text::FontFamily> UiPlugin::getFontFamily(core::Engine& engine, std::string_view name) {
    if (const auto found = fontFamilies.find(name); found != fontFamilies.end()) {
        return found->second;
    }

    std::shared_ptr<text::FontFamily> family;
    if (const auto path = fontPaths.find(name); path != fontPaths.end()) {
        family = std::make_shared<text::FontFamily>(text::FontFamily::Faces{.regular = engine.getAssets().font(path->second), .systemFonts = engine.getPlugin<TextPlugin>().getSystemFonts()});
    } else if (name == ui::Backend::kDefaultFontName) {
        family = engine.getPlugin<TextPlugin>().getRegistry()->getDefaultFamily();
    } else if (name == ui::Backend::kMonospaceFontName) {
        family = std::make_shared<text::FontFamily>(text::FontFamily::Faces{.regular = engine.getPlugin<TextPlugin>().getRegistry()->getDefaultFamily()->getFaces().mono, .systemFonts = engine.getPlugin<TextPlugin>().getSystemFonts()});
    } else {
        return nullptr;
    }

    fontFamilies.emplace(std::string(name), family);
    return family;
}

// Back closes an open popup or dialog, which the UI does itself, and goes back in the navigation of the app while it can.
bool UiPlugin::isCapturingBack() const {
    if (!backend) {
        return false;
    }

    return backend->getImGuiContext()->OpenPopupStack.Size > 0 || navigation->canGoBack();
}

// A GUI lays out as it draws for the first time, so the code of the app that places the world around its nodes finds their bounds only from the next frame. A picture that is still loading leaves its place empty.
bool UiPlugin::isFrameComplete() const {
    return !guiAppeared && !(images && images->isWaiting());
}

bool UiPlugin::isUsingPointer() const {
    return backend && backend->isUsingPointer();
}

bool UiPlugin::isUsingKeyboard() const {
    return backend && backend->isUsingKeyboard();
}

void UiPlugin::installLua(core::Engine&, lua_State* L) {
    ui::UiLua::install(L);
    ui::ImGuiLua::install(L);
}

} // namespace haylen::plugins
