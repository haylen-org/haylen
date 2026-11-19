#include "haylen/ui/Backend.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <utility>

#include <imgui_internal.h>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/System.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/RichText.hpp"
#include "haylen/ui/NavigationInput.hpp"
#include "text/FontFile.hpp"
#include "ui/ImGuiFontLoader.hpp"
#include "ui/ScrollArbiter.hpp"
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
    self.clipboard = self.system.getClipboard();
    return self.clipboard.c_str();
}

void Backend::setClipboardText(ImGuiContext* context, const char* text) {
    getOwner(context).system.setClipboard(text, {});
}

// ImGui copies the data of a callback into its draw list byte by byte, with no alignment, so the data is copied back out the same way.
template <typename Call> Call Backend::readCall(const ImDrawList* list, const ImDrawCmd* command) {
    Call call;
    std::memcpy(&call, list->_CallbacksDataBuf.Data + command->UserCallbackDataOffset, sizeof(call));
    return call;
}

math::Rect Backend::getClip(const ImDrawCmd& command) noexcept {
    return math::Rect::fromMinMax({command.ClipRect.x, command.ClipRect.y}, {command.ClipRect.z, command.ClipRect.w});
}

// Every mesh carries its own render state, so a request to reset it needs no work.
void Backend::resetRenderState(const ImDrawList*, const ImDrawCmd*) {}

// Runs a render callback at its place among the meshes, inside the clip of its command. Its data lives in the draw list, where ImGui points the command only once the frame ends, so a window drawn before that finds it by its offset.
void Backend::runRenderCall(const ImDrawList* list, const ImDrawCmd* command) {
    const auto call = readCall<RenderCall>(list, command);
    Backend& owner = *call.owner;
    const math::Rect clip = getClip(*command);
    if (clip.isEmpty()) {
        return;
    }

    owner.rendering->pushClip(clip);
    owner.renderCalls[call.index](*owner.rendering);
    owner.rendering->popClip();
}

// Draws a child window at its place among the meshes of its parent, before the lists of ImGui reach it.
void Backend::runChildCall(const ImDrawList* list, const ImDrawCmd* command) {
    const auto call = readCall<ChildCall>(list, command);
    call.owner->drawWindow(*call.window);
}

// Draws a run of shapes, texts and nine-slices at its place among the meshes, inside the clip of its command.
void Backend::runDraws(const ImDrawList* list, const ImDrawCmd* command) {
    const auto run = readCall<DrawRun>(list, command);
    Backend& owner = *run.owner;
    const math::Rect clip = getClip(*command);
    if (clip.isEmpty()) {
        return;
    }

    owner.rendering->pushClip(clip);
    for (std::size_t index = run.first; index < run.first + run.count; ++index) {
        owner.runDraw(owner.draws[index]);
    }

    owner.rendering->popClip();
}

void Backend::runPushFade(const ImDrawList* list, const ImDrawCmd* command) {
    const auto call = readCall<FadeCall>(list, command);
    const Fade& fade = call.owner->fades[call.index];
    call.owner->rendering->pushFade(fade.area, fade.edges);
}

void Backend::runPopFade(const ImDrawList* list, const ImDrawCmd* command) {
    const auto call = readCall<FadeCall>(list, command);
    call.owner->rendering->popFade();
}

void Backend::runDraw(const Draw& draw) {
    switch (draw.kind) {
    case Draw::Kind::Shape:
        rendering->drawShape(shapes[draw.index]);
        return;
    case Draw::Kind::Glass:
        rendering->drawGlass(glassSurfaces[draw.index].shape, glassSurfaces[draw.index].glass);
        return;
    case Draw::Kind::Text: {
        const Text& drawn = texts[draw.index];
        rendering->drawText(*drawn.family, getContent(drawn), drawn.origin, drawn.style);
        return;
    }
    case Draw::Kind::RichText: {
        const RichTextDraw& drawn = richTexts[draw.index];
        rendering->drawRichText(*drawn.richText, drawn.origin, {}, drawn.scale, drawn.tint);
        return;
    }
    case Draw::Kind::Sprites: {
        const SpriteDraw& drawn = spriteDraws[draw.index];
        rendering->drawBatch(drawn.texture, std::span(sprites).subspan(drawn.first, drawn.count));
        return;
    }
    }
}

Backend::Backend(graphics::Device& graphicsDevice, platform::Window& hostWindow, platform::System& hostSystem, std::span<const std::uint8_t> defaultFontData, std::span<const std::uint8_t> monospaceFontData) : device(graphicsDevice), window(hostWindow), system(hostSystem), recovery(std::make_unique<Recovery>()), textSession(std::make_unique<TextSession>(hostWindow.getTextInput())), scrollArbiter(std::make_unique<ScrollArbiter>()) {
    imguiContext = ImGui::CreateContext();
    makeCurrent();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.BackendPlatformName = "haylen";
    io.BackendRendererName = "haylen-renderer2d";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasGamepad;
    io.Fonts->SetFontLoader(&ImGuiFontLoader::get());

    // The navigation actions reach ImGui as gamepad keys, so a remapped action moves the focus like its keys, and cancel never drops the focus.
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigNavEscapeClearFocusItem = false;

    // The window switcher of ImGui, which holding the west button of a gamepad or pressing Ctrl+Tab opens, is a tool of ImGui that apps never show.
    imguiContext->ConfigNavWindowingWithGamepad = false;
    imguiContext->ConfigNavWindowingKeyNext = 0;
    imguiContext->ConfigNavWindowingKeyPrev = 0;
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
    ImGui::GetStyle().CircleTessellationMaxError = kCurveTolerance;
    addFont(std::string(kDefaultFontName), {.regular = {defaultFontData.begin(), defaultFontData.end()}});
    addFont(std::string(kMonospaceFontName), {.regular = {monospaceFontData.begin(), monospaceFontData.end()}});
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

math::Rect Backend::getLiftedDisplayRect() const noexcept {
    return getDisplayRect().translated({0.0F, -getKeyboardOffset()});
}

math::Rect Backend::getLiftedSafeRect() const noexcept {
    return safeRect.translated({0.0F, -getKeyboardOffset()});
}

// ImGui only reads a font when it first draws with it, so broken data is caught here while the caller can still hear about it. The height divides the scale of the em square by the scale of the height, as ImGui sizes a face.
float Backend::readHeight(const std::string& name, std::span<const std::uint8_t> data) {
    const std::optional<text::FontFile> file = text::FontFile::read(data);
    if (!file) {
        throw std::runtime_error("The font \"" + name + "\" is not a TrueType or OpenType font.");
    }

    const text::FontFile::LineMetrics& lines = file->getLineMetrics();
    const float emScale = 1.0F / static_cast<float>(file->getUnitsPerEm());
    const float heightScale = 1.0F / static_cast<float>(lines.ascender - lines.descender);
    return emScale / heightScale;
}

ImFont* Backend::addFont(const std::string& name, FontFiles files) {
    makeCurrent();

    if (fonts.contains(name)) {
        throw std::invalid_argument("The UI already has a font named \"" + name + "\".");
    }

    // Every file is checked before any reaches ImGui, so a broken one registers nothing.
    (void)readHeight(name, files.regular);

    for (const std::vector<std::uint8_t>* face : {&files.bold, &files.italic, &files.boldItalic}) {
        if (!face->empty()) {
            (void)readHeight(name, *face);
        }
    }

    for (const std::vector<std::uint8_t>& fallback : files.fallbacks) {
        (void)readHeight(name, fallback);
    }

    // ImGui reads the font data for as long as the atlas lives, so the bytes stay owned here, and every face shares the fallbacks.
    std::vector<std::span<std::uint8_t>> fallbacks;
    for (std::vector<std::uint8_t>& fallback : files.fallbacks) {
        fallbacks.emplace_back(fontData.emplace_back(std::move(fallback)));
    }

    Typeface typeface{.regular = addFace(name, std::move(files.regular), fallbacks)};
    if (!files.bold.empty()) {
        typeface.bold = addFace(name, std::move(files.bold), fallbacks);
    }

    if (!files.italic.empty()) {
        typeface.italic = addFace(name, std::move(files.italic), fallbacks);
    }

    if (!files.boldItalic.empty()) {
        typeface.boldItalic = addFace(name, std::move(files.boldItalic), fallbacks);
    }

    fonts.emplace(name, typeface);
    return typeface.regular.font;
}

// ImGui draws each fallback at the size of the face it merges into, measured from ascent to descent, so a fallback scales by how much its em square differs from the em square of the face.
Backend::Face Backend::addFace(const std::string& name, std::vector<std::uint8_t> bytes, std::span<const std::span<std::uint8_t>> fallbacks) {
    std::vector<std::uint8_t>& data = fontData.emplace_back(std::move(bytes));
    const float height = readHeight(name, data);

    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    ImFont* font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data.data(), static_cast<int>(data.size()), kBaseFontSize, &config);

    for (const std::span<std::uint8_t> fallback : fallbacks) {
        ImFontConfig merged;
        merged.FontDataOwnedByAtlas = false;
        merged.MergeMode = true;
        merged.DstFont = font;
        merged.ExtraSizeScale = readHeight(name, fallback) / height;
        ImGui::GetIO().Fonts->AddFontFromMemoryTTF(fallback.data(), static_cast<int>(fallback.size()), kBaseFontSize, &merged);
    }

    return {.font = font, .height = height};
}

const Backend::Face& Backend::findFace(std::string_view name, bool bold, bool italic) const {
    const auto found = fonts.find(name);
    if (found == fonts.end()) {
        throw std::invalid_argument("The UI has no font named \"" + std::string(name) + "\".");
    }

    const Typeface& typeface = found->second;
    if (bold && italic && typeface.boldItalic.font != nullptr) {
        return typeface.boldItalic;
    }

    if (bold && typeface.bold.font != nullptr) {
        return typeface.bold;
    }

    if (italic && typeface.italic.font != nullptr) {
        return typeface.italic;
    }

    return typeface.regular;
}

ImFont* Backend::getFont(std::string_view name, bool bold, bool italic) const {
    return findFace(name, bold, italic).font;
}

float Backend::getFontHeight(std::string_view name, float size, bool bold, bool italic) const {
    return size * findFace(name, bold, italic).height;
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
        const math::Vec2 min = toUi(viewport.toDesign(event.keyboardFrame.getMin()));
        const math::Vec2 max = toUi(viewport.toDesign(event.keyboardFrame.getMax()));
        textSession->setKeyboardFrame(math::Rect::fromMinMax(min, max));
        break;
    }
    case platform::Event::Type::FocusGained:
        io.AddFocusEvent(true);
        break;
    case platform::Event::Type::FocusLost:
        // A window that loses the focus never hears the fingers leave the trackpad, so they count as gone.
        io.AddFocusEvent(false);
        scrollArbiter->addWheel({}, true, platform::Event::ScrollPhase::Ended);
        break;
    default:
        handlePointer(event, viewport);
        break;
    }
}

// The mouse that a finger drives for the app never reaches ImGui, which follows the finger itself, so a drag of the finger scrolls the area under it.
void Backend::handlePointer(const platform::Event& event, const graphics::Viewport& viewport) {
    if (event.fromTouch) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    // clang-format off
    const auto place = [&](math::Vec2 framebufferPoint) {
        const math::Vec2 point = toUi(viewport.toDesign(framebufferPoint));
        io.AddMousePosEvent(point.x, point.y);
        pointerPosition = framebufferPoint;
        return point;
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
    case platform::Event::Type::MouseScroll: {
        // Shift turns a wheel that only scrolls up and down sideways for the components, as macOS does for every app, and the ImGui windows of the app take the scroll as it came in steps of ImGui, since ImGui turns it with Shift itself.
        const bool sideways = !event.preciseScroll && event.modifiers.shift && event.scroll.x == 0.0F;
        scrollArbiter->addWheel(sideways ? math::Vec2{event.scroll.y, 0.0F} : event.scroll, event.preciseScroll, event.scrollPhase);
        const math::Vec2 steps = event.preciseScroll ? event.scroll / (pointsPerUnit * kImGuiScrollFonts * kBaseFontSize) : event.scroll;
        io.AddMouseWheelEvent(steps.x, steps.y);
        break;
    }
    case platform::Event::Type::MouseLeave:
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        pointerPosition.reset();
        break;
    default:
        break;
    }

    // ImGui follows one pointer, so the first finger down drives it until it lifts and other fingers stay with the app. While a field edits, a finger that presses outside it is held from ImGui, so ImGui never ends the editing at the press: the areas scroll under it and the field keeps editing, and only a finger that lifts within the slop reaches ImGui, as a tap that ends the editing and presses what it tapped.
    const std::span<const platform::TouchPoint> touches(event.touches.data(), event.touchCount);
    switch (event.type) {
    case platform::Event::Type::TouchBegan:
        for (const platform::TouchPoint& touch : touches) {
            if (primaryTouch && touch.changed && touch.id != *primaryTouch) {
                touchShared = true;
            }

            if (!primaryTouch && touch.changed) {
                primaryTouch = touch.id;
                touchShared = false;
                io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
                const math::Vec2 point = place(touch.position);

                if (textSession->getActiveField() != 0 && !textSession->getFieldBounds().contains(point)) {
                    scrollArbiter->holdFinger(point);
                } else {
                    io.AddMouseButtonEvent(0, true);
                }
            }
        }

        break;
    case platform::Event::Type::TouchMoved:
        for (const platform::TouchPoint& touch : touches) {
            if (primaryTouch == touch.id) {
                io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
                scrollArbiter->moveFinger(place(touch.position));
            }
        }

        break;
    case platform::Event::Type::TouchEnded:
    case platform::Event::Type::TouchCancelled:
        for (const platform::TouchPoint& touch : touches) {
            if (primaryTouch != touch.id || !touch.changed) {
                continue;
            }

            // The release happens where the finger lifted, and the pointer leaves with the finger in the next frame, so nothing stays hovered. A held finger that lifts within the slop reaches ImGui there as a tap.
            const bool held = scrollArbiter->isHoldingFinger();
            const bool tapped = scrollArbiter->releaseFinger() && !touchShared && event.type == platform::Event::Type::TouchEnded;
            io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
            place(touch.position);

            if (tapped) {
                io.AddMouseButtonEvent(0, true);
            }

            // ImGui may read the release frames later, so each queued release keeps whether it taps until then.
            if (!held || tapped) {
                io.AddMouseButtonEvent(0, false);
                queuedTouchTaps.push_back(tapped || (!touchShared && event.type == platform::Event::Type::TouchEnded));
            }

            io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
            pointerPosition.reset();
            primaryTouch.reset();
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
        if (!io.WantTextInput) {
            return false;
        }

        for (const input::ActionMap::Binding& binding : navigation.getBindings(action)) {
            if (binding.source == input::ActionMap::Binding::Source::Key && input.isKeyDown(binding.key)) {
                return true;
            }
        }

        return false;
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
    density = viewport.getPixelsPerUnit() * scale;
    pointsPerUnit = std::min(density.x, density.y) / window.getDpiScale();
    origin = visible.getMin();
    safeRect = math::Rect::fromMinMax(toUi(viewport.getSafeRect().getMin()), toUi(viewport.getSafeRect().getMax()));

    // The pointer keeps its place on the screen when design space moves under it, such as after the app changes its scaling.
    if (pointerPosition) {
        const math::Vec2 point = toUi(viewport.toDesign(*pointerPosition));
        io.AddMousePosEvent(point.x, point.y);
    }

    // ImGui measures the tolerance of its curves and the fringe that fades its edges in the pixels of the framebuffer scale, so every curve it draws stays within a fraction of a pixel of the true curve and fades over one pixel.
    io.DisplaySize = {visible.width / scale, visible.height / scale};
    io.DisplayFramebufferScale = {density.x, density.y};
    io.DeltaTime = std::max(deltaSeconds, 1.0F / 1000.0F);

    // ImGui takes in the presses and releases of this frame in its new frame, so a press that lets go now still holds the UI in place for the frame of its release. A finger held from ImGui holds it too, and so do the presses still in the queue of ImGui, such as the tap of a held finger that lifted.
    const bool buttonDown = std::ranges::any_of(io.MouseDown, [](bool down) { return down; });
    const bool buttonQueued = std::ranges::any_of(imguiContext->InputEventsQueue, [](const ImGuiInputEvent& queued) { return queued.Type == ImGuiInputEventType_MouseButton; });
    const bool pointerDown = buttonDown || buttonQueued || scrollArbiter->isHoldingFinger();
    textSession->beginFrame(viewport, scale, deltaSeconds, pointerDown);
    closeAbandonedPopups();
    feedGamepad(input, navigation);

    // The two lists trade places, so the areas of every frame reuse the storage of the frame before the last one.
    std::swap(blockedPrevious, blocked);
    blocked.clear();
    requestedCursor.reset();
    frameTextures.clear();
    renderCalls.clear();
    draws.clear();
    shapes.clear();
    glassSurfaces.clear();
    fades.clear();
    texts.clear();
    characters.clear();
    richTexts.clear();
    spriteDraws.clear();
    sprites.clear();
    renderedLists.clear();
    deferredLists.clear();
    ImGui::NewFrame();
    releaseLiftedTouch();
    dragKeeper = 0;
    immediateWindows.clear();
    scrollArbiter->beginFrame(deltaSeconds);
    findPointerOwner();
    ImGui::ErrorRecoveryStoreState(&recovery->state);
    frameActive = true;
}

// The areas of the last frame tell which GUI the pointer is over now, since the GUIs share one window, where an item or a child window of a GUI drawn first would otherwise keep the pointer from the GUIs drawn over it. A press stays with the GUI that holds it, and a press of no GUI, such as on empty space, leaves the pointer to ImGui, as does a window above the GUIs, such as a window component.
void Backend::findPointerOwner() {
    std::swap(pointerAreasPrevious, pointerAreas);
    pointerAreas.clear();
    pressingGuiPrevious = std::exchange(pressingGui, nullptr);
    pointerOwner.reset();

    const ImGuiContext& state = *imguiContext;
    if (state.HoveredWindow == nullptr || state.HoveredWindow->RootWindow->ID != transparentWindow) {
        return;
    }

    const void* holder = state.ActiveId != 0 ? pressingGuiPrevious : nullptr;
    if (state.ActiveId != 0 && holder == nullptr) {
        return;
    }

    const math::Vec2 pointer{state.IO.MousePos.x, state.IO.MousePos.y};
    for (const PointerArea& candidate : pointerAreasPrevious | std::views::reverse) {
        if (holder != nullptr && candidate.gui != holder) {
            continue;
        }

        if (candidate.area.contains(pointer)) {
            pointerOwner = candidate;
            return;
        }
    }

    // A press that left the areas of its GUI hovers nothing, so its release clicks nothing.
    if (holder != nullptr) {
        pointerOwner = PointerArea{.gui = holder};
    }
}

void Backend::beginGuiPointer(const void* gui) {
    drawingGui = gui;
    ImGuiContext& state = *imguiContext;
    aliveBefore = state.ActiveIdIsAlive;
    if (!pointerOwner) {
        return;
    }

    hoveredWindow = state.HoveredWindow;
    state.HoveredWindow = gui == pointerOwner->gui ? ImGui::FindWindowByID(pointerOwner->window) : nullptr;
}

void Backend::endGuiPointer() {
    ImGuiContext& state = *imguiContext;
    if (aliveBefore == 0 && state.ActiveIdIsAlive != 0) {
        pressingGui = drawingGui;
    }

    if (pointerOwner) {
        state.HoveredWindow = hoveredWindow;
    }

    drawingGui = nullptr;
}

void Backend::takePointer(const math::Rect& area) {
    addPointerArea(area, ImGui::GetCurrentWindow()->ID);
}

// Only the GUIs of the window of app GUIs share their window, and an area counts where its window shows it.
void Backend::addPointerArea(const math::Rect& area, ImGuiID owner) {
    const ImGuiWindow& current = *ImGui::GetCurrentWindow();
    if (drawingGui == nullptr || current.RootWindow->ID != transparentWindow) {
        return;
    }

    const math::Rect clip = math::Rect::fromMinMax({current.ClipRect.Min.x, current.ClipRect.Min.y}, {current.ClipRect.Max.x, current.ClipRect.Max.y});
    const math::Rect shown = area.intersection(clip);
    if (shown.isEmpty()) {
        return;
    }

    pointerAreas.push_back({.area = shown, .gui = drawingGui, .window = owner});
}

math::Rect Backend::getDisplayRect() const noexcept {
    const ImGuiIO& io = imguiContext->IO;
    return {0.0F, 0.0F, io.DisplaySize.x, io.DisplaySize.y};
}

void Backend::setTransparentWindow() {
    transparentWindow = ImGui::GetCurrentWindow()->ID;
}

void Backend::markImmediateWindow() {
    immediateWindows.push_back(ImGui::GetCurrentWindow()->RootWindow->ID);
}

// The item that a finger pressed follows it when the item keeps the drag, when it drags and drops, or when it lies in an ImGui window of the app, which keeps the behavior of ImGui.
bool Backend::isFollowingTouch() const {
    const ImGuiContext& state = *imguiContext;
    if (state.ActiveId == dragKeeper || state.DragDropActive || state.ActiveIdWindow == nullptr) {
        return true;
    }

    return std::ranges::find(immediateWindows, state.ActiveIdWindow->RootWindow->ID) != immediateWindows.end();
}

// A finger that travels past the slop or shares the screen is a swipe, a drag or a pinch, never a tap, so the item it pressed lets it go unless the item follows it, the way a touch screen tells taps from gestures.
void Backend::releaseTravelledTouch() {
    if (!primaryTouch || imguiContext->ActiveId == 0 || isFollowingTouch()) {
        return;
    }

    const ImVec2 moved = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0F);
    if (touchShared || std::hypot(moved.x, moved.y) >= ScrollArbiter::kDragSlop) {
        ImGui::ClearActiveID();
    }
}

// A finger whose release ImGui read in this frame lets go of its item before the item reads it, when the system cancelled the finger, it shared the screen or it travelled past the slop in the last frame it moved. The pointer stays where the finger lifted, so a lifted item lands and a node reports its release there.
void Backend::releaseLiftedTouch() {
    std::optional<bool> tap;
    for (const ImGuiInputEvent& processed : imguiContext->InputEventsTrail) {
        const bool release = processed.Type == ImGuiInputEventType_MouseButton && processed.MouseButton.Button == ImGuiMouseButton_Left && !processed.MouseButton.Down;
        if (release && processed.MouseButton.MouseSource == ImGuiMouseSource_TouchScreen) {
            tap = queuedTouchTaps.front();
            queuedTouchTaps.erase(queuedTouchTaps.begin());
        }
    }

    if (!tap || imguiContext->ActiveId == 0 || isFollowingTouch()) {
        return;
    }

    const ImGuiIO& io = imguiContext->IO;
    const float movedX = io.MousePos.x - io.MouseClickedPos[ImGuiMouseButton_Left].x;
    const float movedY = io.MousePos.y - io.MouseClickedPos[ImGuiMouseButton_Left].y;
    const float travelled = std::max(io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left], (movedX * movedX) + (movedY * movedY));
    if (!*tap || travelled >= ScrollArbiter::kDragSlop * ScrollArbiter::kDragSlop) {
        ImGui::ClearActiveID();
    }
}

void Backend::blockPointer(const math::Rect& area) {
    blocked.push_back(area);
    takePointer(area);
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

bool Backend::beginChild(const char* id, math::Vec2 size, ImGuiChildFlags childFlags, ImGuiWindowFlags windowFlags) {
    const ImGuiWindowFlags inputs = inertDepth > 0 ? ImGuiWindowFlags_NoMouseInputs : ImGuiWindowFlags_None;
    const bool visible = ImGui::BeginChild(id, {size.x, size.y}, childFlags, windowFlags | inputs);
    const ImGuiWindow& child = *ImGui::GetCurrentWindow();
    if (inertDepth == 0) {
        addPointerArea(math::Rect::fromMinMax({child.Pos.x, child.Pos.y}, {child.Pos.x + child.Size.x, child.Pos.y + child.Size.y}), child.ID);
    }
    ChildCall call{.owner = this, .window = &child};
    child.ParentWindow->DrawList->AddCallback(&runChildCall, &call, sizeof(call));
    return visible;
}

void Backend::beginInert() {
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ++inertDepth;
}

void Backend::endInert() {
    --inertDepth;
    ImGui::PopItemFlag();
}

void Backend::pushFade(const Fade& fade) {
    fades.push_back(fade);
    FadeCall call{.owner = this, .index = fades.size() - 1};
    ImGui::GetWindowDrawList()->AddCallback(&runPushFade, &call, sizeof(call));
}

void Backend::popFade() {
    FadeCall call{.owner = this};
    ImGui::GetWindowDrawList()->AddCallback(&runPopFade, &call, sizeof(call));
}

void Backend::addShape(const graphics2d::Shape& shape) {
    shapes.push_back(shape);
    addDraw(Draw::Kind::Shape, shapes.size() - 1);
}

void Backend::addGlass(const graphics2d::Shape& shape, const graphics2d::Glass& glass) {
    glassSurfaces.push_back({.shape = shape, .glass = glass});
    addDraw(Draw::Kind::Glass, glassSurfaces.size() - 1);
}

void Backend::addText(std::shared_ptr<text::FontFamily> family, std::string_view content, math::Vec2 position, const text::Style& style) {
    texts.push_back({.family = std::move(family), .begin = characters.size(), .length = content.size(), .origin = position, .style = style});
    characters.append(content);
    addDraw(Draw::Kind::Text, texts.size() - 1);
}

void Backend::addRichText(std::shared_ptr<text::RichText> richText, math::Vec2 position, math::Vec2 textScale, math::Color tint) {
    richTexts.push_back({.richText = std::move(richText), .origin = position, .scale = textScale, .tint = tint});
    addDraw(Draw::Kind::RichText, richTexts.size() - 1);
}

void Backend::addNineSlice(const graphics2d::NineSlice& slice, const math::Rect& area, float borderScale, math::Vec2 areaScale, math::Vec2 offset, math::Color color) {
    patches.clear();
    slice.layout(area, borderScale, patches);
    const std::size_t first = sprites.size();

    for (const graphics2d::NineSlice::Patch& patch : patches) {
        sprites.push_back({.position = patch.area.getMin() * areaScale + offset, .size = patch.area.getSize() * areaScale, .source = patch.source, .pivot = {}, .color = color});
    }

    spriteDraws.push_back({.texture = slice.texture, .first = first, .count = patches.size()});
    addDraw(Draw::Kind::Sprites, spriteDraws.size() - 1);
}

// A draw that follows the run of the last callback command of the draw list, with nothing drawn and neither the clip nor the texture changed since, joins that run, so a command still tells which texture it shows, such as the image of a nine-slice.
void Backend::addDraw(Draw::Kind kind, std::size_t index) {
    ImDrawList& list = *ImGui::GetWindowDrawList();
    draws.push_back({.kind = kind, .index = index});
    if (list.CmdBuffer.Size >= 2) {
        const ImDrawCmd& current = list.CmdBuffer.back();
        const ImDrawCmd& previous = list.CmdBuffer[list.CmdBuffer.Size - 2];
        const bool sameClip = previous.ClipRect.x == current.ClipRect.x && previous.ClipRect.y == current.ClipRect.y && previous.ClipRect.z == current.ClipRect.z && previous.ClipRect.w == current.ClipRect.w;
        const bool sameTexture = previous.TexRef == current.TexRef;
        if (current.ElemCount == 0 && previous.UserCallback == &runDraws && sameClip && sameTexture) {
            DrawRun run;
            std::memcpy(&run, list._CallbacksDataBuf.Data + previous.UserCallbackDataOffset, sizeof(run));
            if (run.first + run.count == draws.size() - 1) {
                ++run.count;
                std::memcpy(list._CallbacksDataBuf.Data + previous.UserCallbackDataOffset, &run, sizeof(run));
                return;
            }
        }
    }

    DrawRun run{.owner = this, .first = draws.size() - 1, .count = 1};
    list.AddCallback(&runDraws, &run, sizeof(run));
}

ImTextureRef Backend::getTextureReference(const graphics::Texture& texture) {
    const ImTextureID id = kAppTextureBit | texture.getId();

    if (std::ranges::none_of(frameTextures, [&texture](const graphics::Texture& shown) { return shown.getId() == texture.getId(); })) {
        frameTextures.push_back(texture);
    }

    return ImTextureRef(id);
}

const graphics::Texture* Backend::findTexture(ImTextureID id) const {
    if ((id & kAppTextureBit) != 0) {
        const auto shown = std::ranges::find_if(frameTextures, [id](const graphics::Texture& texture) { return (kAppTextureBit | texture.getId()) == id; });
        return shown != frameTextures.end() ? &*shown : nullptr;
    }

    const auto found = atlasTextures.find(id);
    return found != atlasTextures.end() ? &found->second : nullptr;
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

    releaseTravelledTouch();
    ImGui::Render();
    frameActive = false;

    ImDrawData& data = *ImGui::GetDrawData();
    updateTextures(data);

    // Text fields of the UI edit through the text session, while other ImGui text inputs, such as those of `haylen.imgui`, type through the plain keyboard that it holds for them.
    textSession->publish(imguiContext->PlatformImeData.WantTextInput);

    const ImGuiMouseCursor imguiCursor = isUsingPointer() ? ImGui::GetMouseCursor() : ImGuiMouseCursor_Arrow;
    const platform::Window::Cursor wanted = requestedCursor ? *requestedCursor : toCursor(imguiCursor);
    if (wanted != cursor) {
        cursor = wanted;
        window.setCursor(wanted);
    }

    drawLists(renderer, std::span<const ImDrawList* const>(data.CmdLists.Data, static_cast<std::size_t>(data.CmdLists.Size)));
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
}

void Backend::beginDeferring() {
    deferredFrom = imguiContext->WindowsActiveCount;
}

// Every window that began since `beginDeferring` has an order of this frame from that count on, whether it is a child window, a popup or the window of a component.
void Backend::endDeferring() {
    for (const ImGuiWindow* begun : imguiContext->Windows) {
        if (begun->Active && begun->BeginOrderWithinContext >= deferredFrom) {
            deferredLists.push_back(begun->DrawList);
        }
    }
}

// The deferred windows draw in the order of the draw data of the frame, so they cover each other as they would have in it.
void Backend::renderDeferred(graphics2d::Renderer& renderer) {
    if (deferredLists.empty()) {
        return;
    }
    makeCurrent();
    drawnLists.clear();
    const ImDrawData& data = *ImGui::GetDrawData();
    for (const ImDrawList* list : std::span<ImDrawList* const>(data.CmdLists.Data, static_cast<std::size_t>(data.CmdLists.Size))) {
        if (std::ranges::find(deferredLists, list) != deferredLists.end()) {
            drawnLists.push_back(list);
        }
    }
    deferredLists.clear();
    drawLists(renderer, drawnLists);
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
    if (empty && renderCalls.empty() && draws.empty()) {
        return;
    }

    // The canvas maps UI coordinates to the visible area at the scale of the UI, so the meshes of ImGui and the draws of the components keep their UI coordinates. The debug drawings, in the units of the worlds of the app, stay out of it.
    graphics2d::Camera camera;
    camera.anchor = graphics2d::Camera::Anchor::TopLeft;
    camera.setZoom({scale, scale});
    renderer.beginWorld(camera, {.debugDrawings = false});
    rendering = &renderer;

    for (const ImDrawList* list : lists) {
        drawList(*list);
    }

    rendering = nullptr;
}

void Backend::drawWindow(const ImGuiWindow& imguiWindow) {
    if (!imguiWindow.Active || imguiWindow.Hidden) {
        return;
    }

    drawList(*imguiWindow.DrawList);

    for (const ImGuiWindow* child : imguiWindow.DC.ChildWindows) {
        drawWindow(*child);
    }
}

// A list draws once a frame, so a child window drawn inside its parent, or a window drawn before the frame ends, is left out when the lists of ImGui reach it, and a deferred list waits for `renderDeferred`.
void Backend::drawList(const ImDrawList& list) {
    if (std::ranges::find(renderedLists, &list) != renderedLists.end() || std::ranges::find(deferredLists, &list) != deferredLists.end()) {
        return;
    }

    renderedLists.push_back(&list);

    for (const ImDrawCmd& command : list.CmdBuffer) {
        if (command.UserCallback != nullptr) {
            command.UserCallback(&list, &command);
            continue;
        }

        const ImTextureData* atlas = command.TexRef._TexData;
        const graphics::Texture* texture = atlas != nullptr && atlas->TexID == ImTextureID_Invalid ? nullptr : findTexture(command.GetTexID());
        const math::Rect clip = getClip(command);
        if (texture == nullptr || command.ElemCount == 0 || clip.isEmpty()) {
            continue;
        }

        // Only the vertices this command uses are copied, so each mesh uploads exactly what it draws.
        const std::span<const ImDrawIdx> used(list.IdxBuffer.Data + command.IdxOffset, command.ElemCount);
        const auto [lowest, highest] = std::ranges::minmax(used);
        const std::size_t first = command.VtxOffset + lowest;
        meshVertices.clear();
        for (std::size_t index = first; index <= command.VtxOffset + highest; ++index) {
            const ImDrawVert& vertex = list.VtxBuffer[static_cast<int>(index)];
            meshVertices.push_back({.position = {vertex.pos.x, vertex.pos.y}, .uv = {vertex.uv.x, vertex.uv.y}, .color = toColor(vertex.col)});
        }

        meshIndices.clear();
        for (const ImDrawIdx index : used) {
            meshIndices.push_back(static_cast<std::uint32_t>(index - lowest));
        }

        rendering->pushClip(clip);
        // ImGui fades the edges of what it draws itself.
        rendering->drawMesh(*texture, meshVertices, meshIndices, {}, false);
        rendering->popClip();
    }
}

} // namespace haylen::ui
