#include "haylen/ui/Backend.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <stdexcept>
#include <utility>

#include <imgui_internal.h>
#include <stb_truetype.h>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/ui/NavigationInput.hpp"
#include "ui/TextSession.hpp"

namespace haylen::ui {

struct Backend::Recovery {
    ImGuiErrorRecoveryState state;
};

ImGuiKey Backend::toImGuiKey(input::Key key) noexcept {
    const auto code = static_cast<int>(key);
    if (key >= input::Key::A && key <= input::Key::Z) {
        return static_cast<ImGuiKey>(ImGuiKey_A + (code - static_cast<int>(input::Key::A)));
    }
    if (key >= input::Key::Digit0 && key <= input::Key::Digit9) {
        return static_cast<ImGuiKey>(ImGuiKey_0 + (code - static_cast<int>(input::Key::Digit0)));
    }
    if (key >= input::Key::F1 && key <= input::Key::F24) {
        return static_cast<ImGuiKey>(ImGuiKey_F1 + (code - static_cast<int>(input::Key::F1)));
    }
    if (key >= input::Key::Keypad0 && key <= input::Key::Keypad9) {
        return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + (code - static_cast<int>(input::Key::Keypad0)));
    }

    switch (key) {
    case input::Key::Space:
        return ImGuiKey_Space;
    case input::Key::Apostrophe:
        return ImGuiKey_Apostrophe;
    case input::Key::Comma:
        return ImGuiKey_Comma;
    case input::Key::Minus:
        return ImGuiKey_Minus;
    case input::Key::Period:
        return ImGuiKey_Period;
    case input::Key::Slash:
        return ImGuiKey_Slash;
    case input::Key::Semicolon:
        return ImGuiKey_Semicolon;
    case input::Key::Equal:
        return ImGuiKey_Equal;
    case input::Key::LeftBracket:
        return ImGuiKey_LeftBracket;
    case input::Key::Backslash:
        return ImGuiKey_Backslash;
    case input::Key::RightBracket:
        return ImGuiKey_RightBracket;
    case input::Key::GraveAccent:
        return ImGuiKey_GraveAccent;
    case input::Key::Escape:
        return ImGuiKey_Escape;
    case input::Key::Enter:
        return ImGuiKey_Enter;
    case input::Key::Tab:
        return ImGuiKey_Tab;
    case input::Key::Backspace:
        return ImGuiKey_Backspace;
    case input::Key::Insert:
        return ImGuiKey_Insert;
    case input::Key::Delete:
        return ImGuiKey_Delete;
    case input::Key::Right:
        return ImGuiKey_RightArrow;
    case input::Key::Left:
        return ImGuiKey_LeftArrow;
    case input::Key::Down:
        return ImGuiKey_DownArrow;
    case input::Key::Up:
        return ImGuiKey_UpArrow;
    case input::Key::PageUp:
        return ImGuiKey_PageUp;
    case input::Key::PageDown:
        return ImGuiKey_PageDown;
    case input::Key::Home:
        return ImGuiKey_Home;
    case input::Key::End:
        return ImGuiKey_End;
    case input::Key::CapsLock:
        return ImGuiKey_CapsLock;
    case input::Key::ScrollLock:
        return ImGuiKey_ScrollLock;
    case input::Key::NumLock:
        return ImGuiKey_NumLock;
    case input::Key::PrintScreen:
        return ImGuiKey_PrintScreen;
    case input::Key::Pause:
        return ImGuiKey_Pause;
    case input::Key::KeypadDecimal:
        return ImGuiKey_KeypadDecimal;
    case input::Key::KeypadDivide:
        return ImGuiKey_KeypadDivide;
    case input::Key::KeypadMultiply:
        return ImGuiKey_KeypadMultiply;
    case input::Key::KeypadSubtract:
        return ImGuiKey_KeypadSubtract;
    case input::Key::KeypadAdd:
        return ImGuiKey_KeypadAdd;
    case input::Key::KeypadEnter:
        return ImGuiKey_KeypadEnter;
    case input::Key::KeypadEqual:
        return ImGuiKey_KeypadEqual;
    case input::Key::LeftShift:
        return ImGuiKey_LeftShift;
    case input::Key::LeftControl:
        return ImGuiKey_LeftCtrl;
    case input::Key::LeftAlt:
        return ImGuiKey_LeftAlt;
    case input::Key::LeftSuper:
        return ImGuiKey_LeftSuper;
    case input::Key::RightShift:
        return ImGuiKey_RightShift;
    case input::Key::RightControl:
        return ImGuiKey_RightCtrl;
    case input::Key::RightAlt:
        return ImGuiKey_RightAlt;
    case input::Key::RightSuper:
        return ImGuiKey_RightSuper;
    case input::Key::Menu:
        return ImGuiKey_Menu;
    default:
        return ImGuiKey_None;
    }
}

// The keys that edit or leave a focused text input, including the letters of shortcuts such as copy, paste and undo.
bool Backend::isEditKey(ImGuiKey key) noexcept {
    switch (key) {
    case ImGuiKey_Backspace:
    case ImGuiKey_Delete:
    case ImGuiKey_Insert:
    case ImGuiKey_Enter:
    case ImGuiKey_KeypadEnter:
    case ImGuiKey_Tab:
    case ImGuiKey_Escape:
    case ImGuiKey_LeftArrow:
    case ImGuiKey_RightArrow:
    case ImGuiKey_UpArrow:
    case ImGuiKey_DownArrow:
    case ImGuiKey_Home:
    case ImGuiKey_End:
    case ImGuiKey_PageUp:
    case ImGuiKey_PageDown:
        return true;
    default:
        return key >= ImGuiKey_A && key <= ImGuiKey_Z;
    }
}

platform::Window::Cursor Backend::toCursor(ImGuiMouseCursor value) noexcept {
    switch (value) {
    case ImGuiMouseCursor_TextInput:
        return platform::Window::Cursor::IBeam;
    case ImGuiMouseCursor_ResizeAll:
        return platform::Window::Cursor::ResizeAll;
    case ImGuiMouseCursor_ResizeNS:
        return platform::Window::Cursor::ResizeVertical;
    case ImGuiMouseCursor_ResizeEW:
        return platform::Window::Cursor::ResizeHorizontal;
    case ImGuiMouseCursor_ResizeNESW:
        return platform::Window::Cursor::ResizeDiagonalUp;
    case ImGuiMouseCursor_ResizeNWSE:
        return platform::Window::Cursor::ResizeDiagonalDown;
    case ImGuiMouseCursor_Hand:
        return platform::Window::Cursor::PointingHand;
    case ImGuiMouseCursor_NotAllowed:
        return platform::Window::Cursor::NotAllowed;
    default:
        return platform::Window::Cursor::Default;
    }
}

graphics::Image Backend::toImage(ImTextureData& texture) {
    const auto* pixels = static_cast<const std::uint8_t*>(texture.GetPixels());
    const auto count = static_cast<std::size_t>(texture.Width) * static_cast<std::size_t>(texture.Height);
    if (texture.Format == ImTextureFormat_RGBA32) {
        return {texture.Width, texture.Height, std::vector<std::uint8_t>(pixels, pixels + count * 4U)};
    }

    std::vector<std::uint8_t> expanded(count * 4U, 255);
    for (std::size_t index = 0; index < count; ++index) {
        expanded[index * 4U + 3U] = pixels[index];
    }
    return {texture.Width, texture.Height, std::move(expanded)};
}

math::Color Backend::toColor(ImU32 value) noexcept {
    return math::Color::fromRgba8(static_cast<std::uint8_t>(value & 0xFFU), static_cast<std::uint8_t>((value >> 8U) & 0xFFU), static_cast<std::uint8_t>((value >> 16U) & 0xFFU), static_cast<std::uint8_t>(value >> 24U));
}

Backend& Backend::getOwner(ImGuiContext* context) {
    return *static_cast<Backend*>(context->PlatformIO.Platform_ClipboardUserData);
}

const char* Backend::getClipboardText(ImGuiContext* context) {
    Backend& self = getOwner(context);
    self.clipboard = self.window.getClipboard();
    return self.clipboard.c_str();
}

void Backend::setClipboardText(ImGuiContext* context, const char* text) {
    getOwner(context).window.setClipboard(text);
}

// Every mesh carries its own render state, so a request to reset it needs no work.
void Backend::resetRenderState(const ImDrawList*, const ImDrawCmd*) {}

// Runs a render callback at its place among the meshes, inside the clip of its command. Its data lives in the draw list, where ImGui points the command only once the frame ends, so a window drawn before that finds it by its offset.
void Backend::runRenderCall(const ImDrawList* list, const ImDrawCmd* command) {
    const RenderCall& call = *reinterpret_cast<const RenderCall*>(list->_CallbacksDataBuf.Data + command->UserCallbackDataOffset);
    Backend& owner = *call.owner;
    const math::Rect clip = math::Rect::fromMinMax({command->ClipRect.x, command->ClipRect.y}, {command->ClipRect.z, command->ClipRect.w});
    if (clip.isEmpty()) {
        return;
    }
    owner.rendering->pushClip(clip);
    owner.renderCalls[call.index](*owner.rendering);
    owner.rendering->popClip();
}

Backend::Backend(graphics::Device& graphicsDevice, platform::Window& hostWindow, std::span<const std::uint8_t> defaultFontData) : device(graphicsDevice), window(hostWindow), recovery(std::make_unique<Recovery>()), textSession(std::make_unique<TextSession>(hostWindow.getTextInput())) {
    imguiContext = ImGui::CreateContext();
    makeCurrent();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.BackendPlatformName = "haylen";
    io.BackendRendererName = "haylen-renderer2d";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasGamepad;

    // The navigation actions reach ImGui as gamepad keys, so a remapped action moves the focus like its keys, and cancel never drops the focus.
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigNavEscapeClearFocusItem = false;
    io.ConfigErrorRecoveryEnableTooltip = false;
    io.ConfigErrorRecoveryEnableDebugLog = false;

    ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
    platform.Platform_ClipboardUserData = this;
    platform.Platform_GetClipboardTextFn = &getClipboardText;
    platform.Platform_SetClipboardTextFn = &setClipboardText;
    platform.DrawCallback_ResetRenderState = &resetRenderState;
    platform.Renderer_TextureMaxWidth = graphicsDevice.getMaxTextureSize();
    platform.Renderer_TextureMaxHeight = graphicsDevice.getMaxTextureSize();

    ImGui::GetStyle().FontSizeBase = kBaseFontSize;
    addFont(std::string(kDefaultFontName), {.regular = {defaultFontData.begin(), defaultFontData.end()}});
}

Backend::~Backend() {
    ImGui::DestroyContext(imguiContext);
}

void Backend::makeCurrent() const {
    ImGui::SetCurrentContext(imguiContext);
}

TextSession& Backend::getTextSession() noexcept {
    return *textSession;
}

float Backend::getKeyboardOffset() const noexcept {
    return textSession->getKeyboardOffset();
}

// ImGui only parses a font when it first draws with it, so broken data is caught here while the caller can still hear about it.
float Backend::getEmRatio(const std::string& name, std::span<const std::uint8_t> data) {
    stbtt_fontinfo info;
    const int offset = data.size() < kFontHeaderSize ? -1 : stbtt_GetFontOffsetForIndex(data.data(), 0);
    if (offset < 0 || stbtt_InitFont(&info, data.data(), offset) == 0) {
        throw std::runtime_error("The font \"" + name + "\" is not a TrueType or OpenType font.");
    }
    return stbtt_ScaleForPixelHeight(&info, 1.0F) / stbtt_ScaleForMappingEmToPixels(&info, 1.0F);
}

ImFont* Backend::addFont(const std::string& name, FontFiles files) {
    makeCurrent();
    if (fonts.contains(name)) {
        throw std::invalid_argument("The UI already has a font named \"" + name + "\".");
    }

    // Every file is checked before any reaches ImGui, so a broken one registers nothing.
    Typeface typeface{.emRatio = getEmRatio(name, files.regular)};
    for (const std::vector<std::uint8_t>* face : {&files.bold, &files.italic, &files.boldItalic}) {
        if (!face->empty()) {
            (void)getEmRatio(name, *face);
        }
    }
    for (const std::vector<std::uint8_t>& fallback : files.fallbacks) {
        (void)getEmRatio(name, fallback);
    }

    // ImGui reads the font data for as long as the atlas lives, so the bytes stay owned here, and every face shares the fallbacks.
    std::vector<std::span<std::uint8_t>> fallbacks;
    for (std::vector<std::uint8_t>& fallback : files.fallbacks) {
        fallbacks.emplace_back(fontData.emplace_back(std::move(fallback)));
    }
    typeface.regular = addFace(name, std::move(files.regular), fallbacks);
    typeface.bold = files.bold.empty() ? nullptr : addFace(name, std::move(files.bold), fallbacks);
    typeface.italic = files.italic.empty() ? nullptr : addFace(name, std::move(files.italic), fallbacks);
    typeface.boldItalic = files.boldItalic.empty() ? nullptr : addFace(name, std::move(files.boldItalic), fallbacks);
    fonts.emplace(name, typeface);
    return typeface.regular;
}

// ImGui draws each fallback at the size of the face it merges into, measured from ascent to descent, so a fallback scales by how much its em square differs from the em square of the face.
ImFont* Backend::addFace(const std::string& name, std::vector<std::uint8_t> bytes, std::span<const std::span<std::uint8_t>> fallbacks) {
    std::vector<std::uint8_t>& data = fontData.emplace_back(std::move(bytes));
    const float faceRatio = getEmRatio(name, data);
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    ImFont* font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data.data(), static_cast<int>(data.size()), kBaseFontSize, &config);
    for (const std::span<std::uint8_t> fallback : fallbacks) {
        ImFontConfig merged;
        merged.FontDataOwnedByAtlas = false;
        merged.MergeMode = true;
        merged.DstFont = font;
        merged.ExtraSizeScale = faceRatio / getEmRatio(name, fallback);
        ImGui::GetIO().Fonts->AddFontFromMemoryTTF(fallback.data(), static_cast<int>(fallback.size()), kBaseFontSize, &merged);
    }
    return font;
}

ImFont* Backend::getFont(std::string_view name, bool bold, bool italic) const {
    const auto found = fonts.find(name);
    if (found == fonts.end()) {
        throw std::invalid_argument("The UI has no font named \"" + std::string(name) + "\".");
    }
    const Typeface& typeface = found->second;
    if (bold && italic && typeface.boldItalic != nullptr) {
        return typeface.boldItalic;
    }
    if (bold && typeface.bold != nullptr) {
        return typeface.bold;
    }
    if (italic && typeface.italic != nullptr) {
        return typeface.italic;
    }
    return typeface.regular;
}

float Backend::getEmSize(std::string_view name, float size) const {
    const auto found = fonts.find(name);
    if (found == fonts.end()) {
        throw std::invalid_argument("The UI has no font named \"" + std::string(name) + "\".");
    }
    return size * found->second.emRatio;
}

void Backend::handleEvent(const platform::Event& event, const graphics::Viewport& viewport) {
    makeCurrent();
    ImGuiIO& io = ImGui::GetIO();
    switch (event.type) {
    case platform::Event::Type::KeyDown:
    case platform::Event::Type::KeyUp: {
        io.AddKeyEvent(ImGuiMod_Ctrl, event.modifiers.control);
        io.AddKeyEvent(ImGuiMod_Shift, event.modifiers.shift);
        io.AddKeyEvent(ImGuiMod_Alt, event.modifiers.alt);
        io.AddKeyEvent(ImGuiMod_Super, event.modifiers.super);

        // A native field edits the focused text itself, so its editing keys reach ImGui only as the actions the platform reports.
        const ImGuiKey key = toImGuiKey(event.key);
        const bool native = event.type == platform::Event::Type::KeyDown && textSession->isNativeEditing() && isEditKey(key);
        if (key != ImGuiKey_None && !native) {
            io.AddKeyEvent(key, event.type == platform::Event::Type::KeyDown);
        }
        break;
    }
    case platform::Event::Type::Character:
        if (!textSession->isNativeEditing() && event.character >= 32 && event.character != 127) {
            io.AddInputCharacter(static_cast<unsigned int>(event.character));
        }
        break;
    case platform::Event::Type::TextEdited:
        textSession->receiveEdit(event.textEdit);
        break;
    case platform::Event::Type::TextAction:
        handleTextAction(event);
        break;
    case platform::Event::Type::KeyboardChanged: {
        textSession->setKeyboardFrame(math::Rect::fromMinMax(toUi(viewport.toDesign(event.keyboardFrame.getMin())), toUi(viewport.toDesign(event.keyboardFrame.getMax()))));
        break;
    }
    case platform::Event::Type::FocusGained:
    case platform::Event::Type::FocusLost:
        io.AddFocusEvent(event.type == platform::Event::Type::FocusGained);
        break;
    default:
        handlePointer(event, viewport);
        break;
    }
}

void Backend::handlePointer(const platform::Event& event, const graphics::Viewport& viewport) {
    ImGuiIO& io = ImGui::GetIO();
    // clang-format off
    const auto place = [&](math::Vec2 framebufferPoint) {
        const math::Vec2 point = toUi(viewport.toDesign(framebufferPoint));
        io.AddMousePosEvent(point.x, point.y);
        pointerPosition = framebufferPoint;
    };
    // clang-format on

    switch (event.type) {
    case platform::Event::Type::MouseMove:
        io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
        place(event.position);
        break;
    case platform::Event::Type::MouseDown:
    case platform::Event::Type::MouseUp:
        io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
        place(event.position);
        io.AddMouseButtonEvent(static_cast<int>(event.mouseButton), event.type == platform::Event::Type::MouseDown);
        break;
    case platform::Event::Type::MouseScroll:
        io.AddMouseWheelEvent(event.scroll.x, event.scroll.y);
        break;
    case platform::Event::Type::MouseLeave:
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        pointerPosition.reset();
        break;
    default:
        break;
    }

    // ImGui follows one pointer, so the first finger down drives it until it lifts and other fingers stay with the app.
    const std::span<const platform::TouchPoint> touches(event.touches.data(), event.touchCount);
    switch (event.type) {
    case platform::Event::Type::TouchBegan:
        for (const platform::TouchPoint& touch : touches) {
            if (!primaryTouch && touch.changed) {
                primaryTouch = touch.id;
                io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
                place(touch.position);
                io.AddMouseButtonEvent(0, true);
            }
        }
        break;
    case platform::Event::Type::TouchMoved:
        for (const platform::TouchPoint& touch : touches) {
            if (primaryTouch == touch.id) {
                io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
                place(touch.position);
            }
        }
        break;
    case platform::Event::Type::TouchEnded:
    case platform::Event::Type::TouchCancelled:
        for (const platform::TouchPoint& touch : touches) {
            if (primaryTouch == touch.id && touch.changed) {
                io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
                place(touch.position);
                io.AddMouseButtonEvent(0, false);
                primaryTouch.reset();
            }
        }
        break;
    default:
        break;
    }
}

// Moving on presses tab, as a physical keyboard would, while the editor of the field applies a submit, cancel or dismiss itself. A return key labelled `next` moves on too.
void Backend::handleTextAction(const platform::Event& event) {
    const ImGuiID field = textSession->getActiveField();
    if (field == 0 || event.textEdit.field != field) {
        return;
    }

    const platform::TextInput::Action action = event.textAction;
    const bool next = action == platform::TextInput::Action::Next || (action == platform::TextInput::Action::Submit && textSession->getOptions().returnKey == platform::TextInput::ReturnKey::Next);
    if (!next) {
        textSession->receiveAction(action);
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiKey_Tab, true);
    io.AddKeyEvent(ImGuiKey_Tab, false);
}

void Backend::closeAbandonedPopups() {
    const ImGuiContext& state = *GImGui;
    for (int level = 0; level < state.OpenPopupStack.Size; ++level) {
        const ImGuiWindow* window = state.OpenPopupStack[level].Window;
        if (window != nullptr && !window->Active) {
            ImGui::ClosePopupToLevel(level, true);
            return;
        }
    }
}

void Backend::feedGamepad(const input::Input& input, const NavigationInput& navigation) {
    ImGuiIO& io = ImGui::GetIO();
    constexpr std::array<std::pair<NavigationInput::Action, ImGuiKey>, 6> kNavigation{{
        {NavigationInput::Action::Accept, ImGuiKey_GamepadFaceDown},
        {NavigationInput::Action::Cancel, ImGuiKey_GamepadFaceRight},
        {NavigationInput::Action::Left, ImGuiKey_GamepadDpadLeft},
        {NavigationInput::Action::Right, ImGuiKey_GamepadDpadRight},
        {NavigationInput::Action::Up, ImGuiKey_GamepadDpadUp},
        {NavigationInput::Action::Down, ImGuiKey_GamepadDpadDown},
    }};

    // While a text field takes text, the keys that type it never press the navigation actions they are bound to, so Space types a space and Enter starts a new line, while gamepad buttons still accept and cancel.
    // clang-format off
    const auto typed = [&](NavigationInput::Action action) {
        const auto held = [&](const input::ActionMap::Binding& binding) { return binding.source == input::ActionMap::Binding::Source::Key && input.isKeyDown(binding.key); };
        return io.WantTextInput && std::ranges::any_of(navigation.getBindings(action), held);
    };
    // clang-format on
    for (const auto& [action, key] : kNavigation) {
        io.AddKeyEvent(key, navigation.isDown(action) && !typed(action));
    }

    // The other buttons and the triggers come from the first connected gamepad, for the windows ImGui navigates itself.
    std::optional<std::size_t> pad;
    for (std::size_t index = 0; index < input::Input::kMaxGamepads && !pad; ++index) {
        if (input.getGamepad(index).connected) {
            pad = index;
        }
    }
    constexpr std::array<std::pair<input::GamepadButton, ImGuiKey>, 8> kButtons{{
        {input::GamepadButton::West, ImGuiKey_GamepadFaceLeft},
        {input::GamepadButton::North, ImGuiKey_GamepadFaceUp},
        {input::GamepadButton::LeftShoulder, ImGuiKey_GamepadL1},
        {input::GamepadButton::RightShoulder, ImGuiKey_GamepadR1},
        {input::GamepadButton::Back, ImGuiKey_GamepadBack},
        {input::GamepadButton::Start, ImGuiKey_GamepadStart},
        {input::GamepadButton::LeftStick, ImGuiKey_GamepadL3},
        {input::GamepadButton::RightStick, ImGuiKey_GamepadR3},
    }};
    for (const auto& [button, key] : kButtons) {
        io.AddKeyEvent(key, pad && input.isGamepadDown(*pad, button));
    }
    // clang-format off
    const auto analog = [&](ImGuiKey key, float value) {
        const float amount = std::clamp(value, 0.0F, 1.0F);
        io.AddKeyAnalogEvent(key, amount > 0.1F, amount);
    };
    // clang-format on
    analog(ImGuiKey_GamepadL2, pad ? input.getGamepadAxis(*pad, input::GamepadAxis::LeftTrigger) : 0.0F);
    analog(ImGuiKey_GamepadR2, pad ? input.getGamepadAxis(*pad, input::GamepadAxis::RightTrigger) : 0.0F);
}

void Backend::beginFrame(float deltaSeconds, const graphics::Viewport& viewport, const input::Input& input, const NavigationInput& navigation) {
    makeCurrent();
    ImGuiIO& io = ImGui::GetIO();
    if (frameActive) {
        // Recovery closes whatever the failed script left open without raising the assertions it would otherwise hit.
        io.ConfigErrorRecoveryEnableAssert = false;
        ImGui::ErrorRecoveryTryToRecoverState(&recovery->state);
        ImGui::EndFrame();
        io.ConfigErrorRecoveryEnableAssert = true;
        frameActive = false;
    }

    const math::Rect visible = viewport.getVisibleRect();
    const math::Vec2 density = viewport.getPixelsPerUnit() * scale;
    origin = visible.getMin();
    safeRect = math::Rect::fromMinMax(toUi(viewport.getSafeRect().getMin()), toUi(viewport.getSafeRect().getMax()));

    // The pointer keeps its place on the screen when design space moves under it, such as after the app changes its scaling.
    if (pointerPosition) {
        const math::Vec2 point = toUi(viewport.toDesign(*pointerPosition));
        io.AddMousePosEvent(point.x, point.y);
    }

    io.DisplaySize = {visible.width / scale, visible.height / scale};
    io.DisplayFramebufferScale = {density.x, density.y};
    io.DeltaTime = std::max(deltaSeconds, 1.0F / 1000.0F);
    textSession->beginFrame(viewport, scale, deltaSeconds);
    closeAbandonedPopups();
    feedGamepad(input, navigation);

    // The two lists trade places, so the areas of every frame reuse the storage of the frame before the last one.
    std::swap(blockedPrevious, blocked);
    blocked.clear();
    requestedCursor.reset();
    frameTextures.clear();
    renderCalls.clear();
    renderedLists.clear();
    ImGui::NewFrame();
    ImGui::ErrorRecoveryStoreState(&recovery->state);
    frameActive = true;
}

math::Rect Backend::getDisplayRect() const noexcept {
    const ImGuiIO& io = imguiContext->IO;
    return {0.0F, 0.0F, io.DisplaySize.x, io.DisplaySize.y};
}

void Backend::setTransparentWindow() {
    transparentWindow = ImGui::GetCurrentWindow()->ID;
}

void Backend::blockPointer(const math::Rect& area) {
    blocked.push_back(area);
}

bool Backend::isUsingPointer() const {
    const ImGuiContext& state = *imguiContext;
    if (!state.IO.WantCaptureMouse) {
        return false;
    }
    if (state.HoveredWindow == nullptr || state.HoveredWindow->ID != transparentWindow) {
        return true;
    }

    // Over the transparent GUI window only items and the areas components blocked in the last frame keep the pointer from the app. A press on empty space makes ImGui hold the move id of that window, which is no item.
    const math::Vec2 pointer{state.IO.MousePos.x, state.IO.MousePos.y};
    const auto covers = [pointer](const math::Rect& area) { return area.contains(pointer); };
    const bool activeItem = state.ActiveId != 0 && state.ActiveId != state.HoveredWindow->MoveId;
    return state.HoveredIdPreviousFrame != 0 || activeItem || std::ranges::any_of(blockedPrevious, covers);
}

bool Backend::isUsingKeyboard() const {
    return imguiContext->IO.WantCaptureKeyboard;
}

void Backend::addRenderCallback(std::function<void(graphics2d::Renderer& renderer)> draw) {
    renderCalls.push_back(std::move(draw));
    RenderCall call{.owner = this, .index = renderCalls.size() - 1};
    ImGui::GetWindowDrawList()->AddCallback(&runRenderCall, &call, sizeof(call));
}

ImTextureRef Backend::getTextureReference(const graphics::Texture& texture) {
    const ImTextureID id = kAppTextureBit | texture.getId();
    frameTextures.insert_or_assign(id, texture);
    return ImTextureRef(id);
}

const graphics::Texture* Backend::findTexture(ImTextureID id) const {
    const auto& textures = (id & kAppTextureBit) != 0 ? frameTextures : atlasTextures;
    const auto found = textures.find(id);
    return found != textures.end() ? &found->second : nullptr;
}

void Backend::updateTextures(ImDrawData& data) {
    if (data.Textures == nullptr) {
        return;
    }
    for (ImTextureData* texture : *data.Textures) {
        switch (texture->Status) {
        case ImTextureStatus_WantCreate: {
            const ImTextureID id = nextAtlasTexture++;
            atlasTextures.emplace(id, device.createDynamicTexture(toImage(*texture), {.filter = graphics::Texture::Filter::Linear}));
            texture->SetTexID(id);
            texture->SetStatus(ImTextureStatus_OK);
            break;
        }
        case ImTextureStatus_WantUpdates:
            device.updateTexture(atlasTextures.at(texture->TexID), toImage(*texture).getPixels());
            texture->SetStatus(ImTextureStatus_OK);
            break;
        case ImTextureStatus_WantDestroy:
            if (texture->UnusedFrames > 0) {
                atlasTextures.erase(texture->TexID);
                texture->SetTexID(ImTextureID_Invalid);
                texture->SetStatus(ImTextureStatus_Destroyed);
            }
            break;
        default:
            break;
        }
    }
}

void Backend::render(graphics2d::Renderer& renderer) {
    makeCurrent();
    if (!frameActive) {
        return;
    }
    ImGui::Render();
    frameActive = false;

    ImDrawData& data = *ImGui::GetDrawData();
    updateTextures(data);

    // Text fields of the UI edit through the text session, while other ImGui text inputs, such as those of `haylen.imgui`, type through the plain keyboard of the window.
    const bool plainInput = imguiContext->PlatformImeData.WantTextInput && !textSession->isActive();
    if (keyboardShown && !plainInput) {
        keyboardShown = false;
        window.setKeyboardVisible(false);
    }
    textSession->publish();
    if (plainInput && !keyboardShown) {
        keyboardShown = true;
        window.setKeyboardVisible(true);
    }

    const platform::Window::Cursor wanted = requestedCursor ? *requestedCursor : toCursor(isUsingPointer() ? ImGui::GetMouseCursor() : ImGuiMouseCursor_Arrow);
    if (wanted != cursor) {
        cursor = wanted;
        window.setCursor(wanted);
    }

    drawnLists.clear();
    for (const ImDrawList* list : data.CmdLists) {
        if (std::ranges::find(renderedLists, list) == renderedLists.end()) {
            drawnLists.push_back(list);
        }
    }
    drawLists(renderer, drawnLists);
}

void Backend::renderWindow(graphics2d::Renderer& renderer, const ImGuiWindow& root) {
    makeCurrent();
    drawnLists.clear();
    collectLists(root, drawnLists);
    for (const ImGuiPopupData& popup : GImGui->OpenPopupStack) {
        if (popup.Window != nullptr && popup.Window->RootWindowPopupTree == &root && popup.Window != &root) {
            collectLists(*popup.Window, drawnLists);
        }
    }
    drawLists(renderer, drawnLists);
    renderedLists.insert(renderedLists.end(), drawnLists.begin(), drawnLists.end());
}

void Backend::collectLists(const ImGuiWindow& window, std::vector<const ImDrawList*>& lists) {
    if (!window.Active || window.Hidden) {
        return;
    }
    lists.push_back(window.DrawList);
    for (const ImGuiWindow* child : window.DC.ChildWindows) {
        collectLists(*child, lists);
    }
}

// A command whose texture never reached the GPU, such as an atlas that the frame creates and a window drawn before the frame ends uses, draws nothing.
void Backend::drawLists(graphics2d::Renderer& renderer, std::span<const ImDrawList* const> lists) {
    const bool empty = std::ranges::all_of(lists, [](const ImDrawList* list) { return list->VtxBuffer.Size == 0; });
    if (empty && renderCalls.empty()) {
        return;
    }
    // The canvas maps UI coordinates to the visible area at the scale of the UI, so the meshes of ImGui and the draws of the components keep their UI coordinates.
    graphics2d::Camera camera;
    camera.anchor = graphics2d::Camera::Anchor::TopLeft;
    camera.setZoom({scale, scale});
    renderer.beginWorld(camera);
    rendering = &renderer;
    for (const ImDrawList* list : lists) {
        for (const ImDrawCmd& command : list->CmdBuffer) {
            if (command.UserCallback != nullptr) {
                command.UserCallback(list, &command);
                continue;
            }

            const ImTextureData* atlas = command.TexRef._TexData;
            const graphics::Texture* texture = atlas != nullptr && atlas->TexID == ImTextureID_Invalid ? nullptr : findTexture(command.GetTexID());
            const math::Rect clip = math::Rect::fromMinMax({command.ClipRect.x, command.ClipRect.y}, {command.ClipRect.z, command.ClipRect.w});
            if (texture == nullptr || command.ElemCount == 0 || clip.isEmpty()) {
                continue;
            }

            // Only the vertices this command uses are copied, so each mesh uploads exactly what it draws.
            const std::span<const ImDrawIdx> used(list->IdxBuffer.Data + command.IdxOffset, command.ElemCount);
            const auto [lowest, highest] = std::ranges::minmax(used);
            const std::size_t first = command.VtxOffset + lowest;
            meshVertices.clear();
            for (std::size_t index = first; index <= command.VtxOffset + highest; ++index) {
                const ImDrawVert& vertex = list->VtxBuffer[static_cast<int>(index)];
                meshVertices.push_back({.position = {vertex.pos.x, vertex.pos.y}, .uv = {vertex.uv.x, vertex.uv.y}, .color = toColor(vertex.col)});
            }
            meshIndices.clear();
            for (const ImDrawIdx index : used) {
                meshIndices.push_back(static_cast<std::uint32_t>(index - lowest));
            }

            renderer.pushClip(clip);
            renderer.drawMesh(*texture, meshVertices, meshIndices);
            renderer.popClip();
        }
    }
    rendering = nullptr;
}

} // namespace haylen::ui
