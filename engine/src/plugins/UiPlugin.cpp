#include "haylen/plugins/UiPlugin.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <string>
#include <utility>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/EmbeddedFiles.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/core/SceneView.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "haylen/localization/Catalog.hpp"
#include "haylen/plugins/LocalizationPlugin.hpp"
#include "haylen/plugins/TextPlugin.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/ImGuiLua.hpp"
#include "ui/UiLua.hpp"
#include "ui/components/BuiltInComponents.hpp"

namespace haylen::plugins {

UiPlugin::UiPlugin() {
    ui::BuiltInComponents::registerAll(components);
    themes.emplace("dark", ui::Theme::dark());
    themes.emplace("light", ui::Theme::light());
}

UiPlugin::~UiPlugin() = default;

void UiPlugin::start(core::Engine& engine) {
    owner = &engine;
    backend = std::make_unique<ui::Backend>(engine.getGraphics(), engine.getWindow(), core::EmbeddedFiles::getDefaultFont());
    safeAreaVisible = engine.getConfig().debug.showSafeArea;
    // clang-format off
    context = std::make_unique<ui::Context>(*backend, focus, engine.getPlugin<LocalizationPlugin>().getCatalog(), engine.getInput(), [this](std::string_view path) {
        return requestImage(*owner, path);
    }, [this](std::string_view name) {
        return getFontFamily(*owner, name);
    }, engine.getPlugin<TextPlugin>().getRegistry());
    // clang-format on
    setTheme(themeName);
}

void UiPlugin::stop(core::Engine& engine) {
    input::VirtualInput& controls = engine.getVirtualInput();
    for (const std::string& name : std::exchange(heldButtons, {})) {
        controls.setButton(name, false);
    }
    for (const std::string& name : std::exchange(sticks, {})) {
        controls.setStick(name, {});
    }
    engine.getInput().setPointerCaptured(false);
    engine.getActions().setCapture({});

    // Pending image loads finish into a cache that no longer exists, so they are told to drop their result.
    alive = std::make_shared<bool>(true);
    images.clear();
    fontPaths.clear();
    fontFamilies.clear();

    // No document is left to report events, and listeners holding Lua functions let go before the Lua state closes.
    documents.clear();
    events.clear();
    context.reset();
    backend.reset();
    owner = nullptr;
}

ui::Backend& UiPlugin::getBackend() {
    if (!backend) {
        throw std::logic_error("The UI plugin has not started.");
    }
    return *backend;
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
    // The UI keeps real time, so menus still animate while the time scale pauses the world.
    const auto delta = static_cast<float>(engine.getClock().getUnscaledDelta());
    elapsed += delta;
    drawBegun = false;
    navigation.update(engine.getActions(), engine.getInput(), engine.getVirtualInput(), engine.isHalted() || engine.getScenes().isInputBlocked(), focus.getOwner() == ui::FocusNavigator::Owner::PlayArea);
    backend->beginFrame(delta, engine.getViewport(), engine.getInput(), navigation);
    context->beginFrame(elapsed, delta, engine.getViewport().getVisibleRect().getMin());

    // Text reads in the current language, and an automatic direction takes the one that language declares.
    const localization::Catalog& catalog = engine.getPlugin<LocalizationPlugin>().getCatalog();
    const bool known = !catalog.getLanguage().empty();
    const text::Direction resolved = direction == text::Direction::Auto ? (known ? catalog.getDirection(catalog.getLanguage()) : text::Direction::LeftToRight) : direction;
    context->setBaseWriting(resolved, catalog.getLanguage());
    focus.update(navigation, engine.getInput().getLastDevice(), engine.getWindow().hasPointerDevice());
}

void UiPlugin::update(core::Engine& engine, float) {
    // Handlers may mount or unmount documents, so the loop walks a copy and skips documents that are gone.
    const std::vector<Mounted> snapshot = documents;
    for (const Mounted& entry : snapshot) {
        ui::Document& document = *entry.document;
        for (const ui::Event& event : document.takeEvents()) {
            if (!isMounted(document)) {
                break;
            }
            events.emit(document, event);
            ui::UiLua::deliverEvent(engine.getLuaState(), document, event);
        }
    }
}

// Every view draws the documents of its scenes, a view that leaves through a transition into its own image before the frame ends, and the current view also the documents that belong to no scene, which ends the frame.
void UiPlugin::renderUi(core::Engine& engine, const core::SceneView& view) {
    ui::Backend& drawing = getBackend();
    if (!drawing.isFrameActive()) {
        return;
    }
    if (!drawBegun) {
        drawBegun = true;
        shownDocuments.clear();
        focus.beginDraw();
    }
    if (view.current) {
        drawCurrent(engine, view);
    } else if (!view.scenes.empty()) {
        drawLeaving(engine, view);
    }
}

// The documents a view draws in layer order: those of its scenes, and in the current view those of no scene too. A scene in the current view and in a leaving view, such as one under a transparent scene pushed through an effect that shows both, keeps its documents in the current view.
std::vector<const UiPlugin::Mounted*> UiPlugin::getDocuments(core::Engine& engine, const core::SceneView& view) const {
    const std::vector<core::SceneView>& views = engine.getScenes().getViews();
    const auto current = std::ranges::find_if(views, &core::SceneView::current);
    // clang-format off
    const auto shows = [](const core::SceneView& candidate, const core::Scene* scene) {
        return std::ranges::any_of(candidate.scenes, [scene](const std::shared_ptr<core::Scene>& shown) { return shown.get() == scene; });
    };
    // clang-format on

    std::vector<const Mounted*> drawn;
    for (const Mounted& entry : documents) {
        if (!entry.scened) {
            if (view.current) {
                drawn.push_back(&entry);
            }
            continue;
        }
        const std::shared_ptr<const core::Scene> scene = entry.scene.lock();
        if (scene && shows(view, scene.get()) && (view.current || current == views.end() || !shows(*current, scene.get()))) {
            drawn.push_back(&entry);
        }
    }
    std::ranges::sort(drawn, [](const Mounted* lhs, const Mounted* rhs) { return lhs->layer != rhs->layer ? lhs->layer < rhs->layer : lhs->order < rhs->order; });
    return drawn;
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

// Every document moves up together while the on-screen keyboard would cover the focused text field.
void UiPlugin::drawDocuments(const std::vector<const Mounted*>& drawn) {
    ui::Backend& drawing = getBackend();
    const math::Rect display = drawing.getDisplayRect();
    const math::Vec2 lift{0.0F, -drawing.getKeyboardOffset()};
    for (const Mounted* entry : drawn) {
        ImGui::PushID(entry->document.get());
        entry->document->draw(*context, (entry->document->getPlacement() == ui::Placement::Safe ? drawing.getSafeRect() : display).translated(lift));
        ImGui::PopID();
        shownDocuments.push_back(entry->document.get());
    }
}

// The documents of a leaving view take no input and no focus, and they draw into the image of their view right away.
void UiPlugin::drawLeaving(core::Engine& engine, const core::SceneView& view) {
    beginWindow("##haylen-leaving", ImGuiWindowFlags_NoInputs);
    focus.suspendTargets(true);
    drawDocuments(getDocuments(engine, view));
    focus.suspendTargets(false);
    const ImGuiWindow& window = *ImGui::GetCurrentWindow();
    ImGui::End();
    getBackend().renderWindow(engine.getRenderer2D(), window);
}

void UiPlugin::drawCurrent(core::Engine& engine, const core::SceneView& view) {
    ui::Backend& drawing = getBackend();
    beginWindow("##haylen-documents", ImGuiWindowFlags_NoNavInputs);
    drawing.setTransparentWindow();
    drawDocuments(getDocuments(engine, view));

    // The documents that no view shows, such as those of a covered scene, hear that they stopped drawing.
    for (const Mounted& entry : documents) {
        if (std::ranges::find(shownDocuments, entry.document.get()) == shownDocuments.end()) {
            entry.document->skip(*context);
        }
    }
    ImGui::End();
    focus.endDraw();
    if (safeAreaVisible) {
        drawSafeArea();
    }

    drawing.render(engine.getRenderer2D());
    applyVirtualInput(engine);

    // The action map of the next frame ignores mouse buttons while the interface owns the pointer, so a click on a button never reaches gameplay, and it leaves the presses the interface answers itself to the interface, such as the cancel that closes a popup.
    engine.getInput().setPointerCaptured(isUsingPointer());
    engine.getActions().setCapture(getCapture());
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
        {Action::Cancel, focus.answersCancel()},
        {Action::Left, control},
        {Action::Right, control},
        {Action::Up, control},
        {Action::Down, control},
        {Action::Menu, control},
        {Action::Focus, focus.answersFocus()},
    }};
    for (const auto& [action, answered] : answers) {
        if (answered) {
            addCapture(capture, navigation.getBindings(action));
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

    const std::string label = std::format("Safe area {:.0f} {:.0f} {:.0f} {:.0f}", safe.y - display.y, display.getRight() - safe.getRight(), display.getBottom() - safe.getBottom(), safe.x - display.x);
    list.AddText({safe.x + 12.0F, safe.y + 8.0F}, line, label.c_str());
}

void UiPlugin::applyVirtualInput(core::Engine& engine) {
    input::VirtualInput& controls = engine.getVirtualInput();
    for (const std::string& name : heldButtons) {
        if (!context->getHeldButtons().contains(name)) {
            controls.setButton(name, false);
        }
    }
    for (const std::string& name : context->getHeldButtons()) {
        controls.setButton(name, true);
    }
    for (const std::string& name : sticks) {
        if (!context->getSticks().contains(name)) {
            controls.setStick(name, {});
        }
    }

    heldButtons = context->getHeldButtons();
    sticks.clear();
    for (const auto& [name, value] : context->getSticks()) {
        controls.setStick(name, value);
        sticks.insert(name);
    }
}

std::shared_ptr<ui::Document> UiPlugin::createDocument(const core::Json& tree, ui::Placement placement) const {
    return std::make_shared<ui::Document>(components, tree, placement);
}

void UiPlugin::mount(std::shared_ptr<ui::Document> document, int layer, const std::shared_ptr<const core::Scene>& scene) {
    if (owner == nullptr) {
        throw std::logic_error("The UI plugin has not started.");
    }
    if (!document || isMounted(*document)) {
        throw std::invalid_argument("Only a document that is not mounted can be mounted.");
    }
    documents.push_back({.document = document, .scene = scene, .scened = scene != nullptr, .layer = layer, .order = nextOrder++});
    owner->getEvents().emitWith(core::LifecycleEvent::kUiDocumentMounted, document);
}

// Documents are only mounted while the plugin runs, so a document that is found has an engine to report to.
bool UiPlugin::unmount(const ui::Document& document) {
    const auto found = std::ranges::find_if(documents, [&](const Mounted& entry) { return entry.document.get() == &document; });
    if (found == documents.end()) {
        return false;
    }
    const std::shared_ptr<ui::Document> unmounted = found->document;
    documents.erase(found);
    focus.forget(*unmounted);
    owner->getEvents().emitWith(core::LifecycleEvent::kUiDocumentUnmounted, unmounted);
    ui::UiLua::forgetDocument(owner->getLuaState(), document);
    return true;
}

bool UiPlugin::isMounted(const ui::Document& document) const {
    return std::ranges::any_of(documents, [&](const Mounted& entry) { return entry.document.get() == &document; });
}

std::shared_ptr<ui::Document> UiPlugin::findMounted(const ui::Document* document) const {
    const auto found = std::ranges::find_if(documents, [&](const Mounted& entry) { return entry.document.get() == document; });
    return found != documents.end() ? found->document : nullptr;
}

void UiPlugin::addTheme(ui::Theme theme) {
    const std::string name = theme.getName();
    themes.insert_or_assign(name, std::move(theme));
    if (name == themeName && backend) {
        setTheme(name);
    }
}

void UiPlugin::setTheme(std::string_view name) {
    const auto found = themes.find(name);
    if (found == themes.end()) {
        throw std::invalid_argument("The UI has no theme named \"" + std::string(name) + "\".");
    }
    themeName = found->first;
    if (!backend) {
        return;
    }

    const ui::Theme& theme = found->second;
    for (std::size_t index = 0; index < ui::Theme::kFontCount; ++index) {
        (void)backend->getFont(theme.getFont(static_cast<ui::Theme::Font>(index)).font);
    }
    backend->makeCurrent();
    theme.applyTo(ImGui::GetStyle());
    ImGui::GetIO().FontDefault = backend->getFont(theme.getFont(ui::Theme::Font::Body).font);
    context->setTheme(theme);
}

const ui::Theme& UiPlugin::getTheme() const {
    return themes.find(themeName)->second;
}

std::vector<std::string> UiPlugin::getThemes() const {
    std::vector<std::string> names;
    for (const auto& [name, theme] : themes) {
        names.push_back(name);
    }
    return names;
}

std::string UiPlugin::addTheme(core::Engine& engine, const core::Json& document, std::string_view base) {
    const auto baseTheme = themes.find(base);
    if (baseTheme == themes.end()) {
        throw std::invalid_argument("The UI has no theme named \"" + std::string(base) + "\" to start from.");
    }

    // clang-format off
    ui::Theme theme = ui::Theme::fromJson(document, baseTheme->second, [&engine](std::string_view texture, graphics::Texture::Options options) {
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

    std::string name = theme.getName();
    addTheme(std::move(theme));
    return name;
}

std::string UiPlugin::loadTheme(core::Engine& engine, std::string_view path, std::string_view base) {
    return addTheme(engine, engine.getAssets().json(path), base);
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

// A font registered from a file becomes a family with that font as its regular face the first time rich text asks for it.
std::shared_ptr<text::FontFamily> UiPlugin::getFontFamily(core::Engine& engine, std::string_view name) {
    if (const auto found = fontFamilies.find(name); found != fontFamilies.end()) {
        return found->second;
    }
    std::shared_ptr<text::FontFamily> family;
    if (const auto path = fontPaths.find(name); path != fontPaths.end()) {
        family = std::make_shared<text::FontFamily>(text::FontFamily::Faces{.regular = engine.getAssets().font(path->second)});
    } else if (name == ui::Backend::kDefaultFontName) {
        family = engine.getPlugin<TextPlugin>().getRegistry()->getDefaultFamily();
    } else {
        return nullptr;
    }
    fontFamilies.emplace(std::string(name), family);
    return family;
}

// Back closes an open popup or dialog, which the UI does itself.
bool UiPlugin::isCapturingBack() const {
    return backend && backend->getImGuiContext()->OpenPopupStack.Size > 0;
}

bool UiPlugin::isUsingPointer() const {
    return backend && backend->isUsingPointer();
}

bool UiPlugin::isUsingKeyboard() const {
    return backend && backend->isUsingKeyboard();
}

graphics::Texture UiPlugin::requestImage(core::Engine& engine, std::string_view path) {
    const graphics::Texture::Filter filter = getTheme().getImageFilter();
    if (const auto found = images.find(path); found != images.end() && found->second.filter == filter) {
        if (!found->second.error.empty()) {
            throw std::runtime_error("The UI image \"" + std::string(path) + "\" could not be loaded. " + found->second.error);
        }
        return found->second.texture;
    }

    // Images load in the background, and a component draws nothing in their place until they arrive. A theme with another image filter loads them again, and an answer for the filter of an earlier theme goes nowhere.
    images.insert_or_assign(std::string(path), ImageEntry{.texture = {}, .error = {}, .filter = filter});
    // clang-format off
    engine.getAssets().textureAsync(path, [this, weakAlive = std::weak_ptr<bool>(alive), key = std::string(path), filter](graphics::Texture texture, std::string error) {
        if (weakAlive.expired()) {
            return;
        }
        ImageEntry& entry = images[key];
        if (entry.filter != filter) {
            return;
        }
        entry.texture = std::move(texture);
        entry.error = std::move(error);
    }, {.filter = filter, .wrap = graphics::Texture::Wrap::Clamp});
    // clang-format on
    return {};
}

void UiPlugin::installLua(core::Engine&, lua_State* L) {
    ui::UiLua::install(L);
    ui::ImGuiLua::install(L);
}

} // namespace haylen::plugins
