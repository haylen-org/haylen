#include "core/ErrorScreen.hpp"

#include <algorithm>
#include <exception>
#include <utility>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/core/Version.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/plugins/HotReloadPlugin.hpp"
#include "haylen/text/Font.hpp"

namespace haylen::core {

ErrorScreen::ErrorScreen(Engine& owner, lua::Error failure) : engine(owner), error(std::move(failure)) {
    excerpt = readExcerpt();
    report = buildReport();
}

std::string ErrorScreen::expandTabs(std::string_view text) {
    std::string expanded;
    std::size_t column = 0;
    for (const char character : text) {
        if (character == '\t') {
            const std::size_t spaces = kTabWidth - column % kTabWidth;
            expanded.append(spaces, ' ');
            column += spaces;
            continue;
        }
        if (character == '\r') {
            continue;
        }
        expanded.push_back(character);
        column = character == '\n' ? 0 : column + 1;
    }
    return expanded;
}

std::vector<ErrorScreen::SourceLine> ErrorScreen::readExcerpt() const {
    const std::string& file = error.getFile();
    io::Package& package = engine.getPackage();
    if (file.empty() || error.getLine() <= 0 || !package.exists(file)) {
        return {};
    }

    // The screen explains an error that already happened, so a package that cannot be read now only costs it the excerpt.
    std::string source;
    try {
        source = package.readText(file);
    } catch (const std::exception& exception) {
        Log::warning("The error screen shows no source excerpt, because \"{}\" could not be read: {}", file, exception.what());
        return {};
    }

    std::vector<SourceLine> lines;
    const int first = std::max(1, error.getLine() - kExcerptRadius);
    const int last = error.getLine() + kExcerptRadius;
    int number = 1;
    for (std::size_t start = 0; start < source.size() && number <= last; ++number) {
        const std::size_t end = std::min(source.find('\n', start), source.size());
        if (number >= first) {
            lines.push_back({.number = number, .text = expandTabs(std::string_view(source).substr(start, end - start))});
        }
        start = end + 1;
    }
    if (lines.empty() || lines.back().number < error.getLine()) {
        return {};
    }
    return lines;
}

std::string ErrorScreen::getDetails() const {
    const AppConfig& config = engine.getConfig();
    return config.name + " " + config.version + " · " + std::string(engine.getPlatformName()) + " · Haylen " + std::string(Version::kString);
}

std::string ErrorScreen::getLocation() const {
    return "File \"" + error.getFile() + "\", line " + std::to_string(error.getLine());
}

std::string ErrorScreen::buildReport() const {
    std::string text = std::string(kTitle) + "\n" + getDetails() + "\n\n" + expandTabs(error.getMessage());
    if (!error.getFile().empty()) {
        text += "\n" + getLocation();
    }

    if (!excerpt.empty()) {
        text += "\n";
        const std::size_t width = std::to_string(excerpt.back().number).size();
        for (const SourceLine& line : excerpt) {
            const std::string number = std::to_string(line.number);
            text += "\n" + std::string(line.number == error.getLine() ? "> " : "  ") + std::string(width - number.size(), ' ') + number;
            if (!line.text.empty()) {
                text += "  " + line.text;
            }
        }
    }

    if (!error.getFrames().empty()) {
        text += "\n\nStack\n" + error.getTraceback();
    }
    return text;
}

bool ErrorScreen::isTouchDevice() const {
    const std::string_view platform = engine.getPlatformName();
    return platform == "android" || platform == "ios" || engine.getInput().getLastDevice() == input::InputDevice::Touch;
}

// A gamepad drives the screen once it was the last device, and on TVs, which have no pointer, from the start.
bool ErrorScreen::isGamepadDriven() const {
    return engine.getInput().getLastDevice() == input::InputDevice::Gamepad || !engine.getWindow().hasPointerDevice();
}

bool ErrorScreen::isReloadWatching() const {
    const auto* hotReload = engine.getPlugins().find<plugins::HotReloadPlugin>();
    return hotReload != nullptr && hotReload->isWatching();
}

float ErrorScreen::getUnit() const {
    return engine.getWindow().getDpiScale() / engine.getViewport().getPixelsPerUnit().y;
}

math::Vec2 ErrorScreen::toCanvas(math::Vec2 framebufferPoint) const {
    return engine.getViewport().toDesign(framebufferPoint);
}

void ErrorScreen::copyReport() {
    engine.getWindow().setClipboard(report);
    copied = true;
}

void ErrorScreen::restartApp() {
    engine.requestRestart();
}

void ErrorScreen::scrollBy(float amount) {
    scroll = std::clamp(scroll + amount, 0.0F, maxScroll);
}

void ErrorScreen::handleKey(const platform::Event& event) {
    const float line = kLineStep * getUnit();
    switch (event.key) {
    case input::Key::C:
        if (!event.repeat) {
            copyReport();
        }
        break;
    case input::Key::R:
        if (!event.repeat) {
            restartApp();
        }
        break;
    case input::Key::Left:
    case input::Key::Right:
    case input::Key::Enter:
        pressFocus(event);
        break;
    case input::Key::Up:
        scrollBy(-line);
        break;
    case input::Key::Down:
        scrollBy(line);
        break;
    case input::Key::PageUp:
        scrollBy(-page);
        break;
    case input::Key::PageDown:
        scrollBy(page);
        break;
    case input::Key::Home:
        scroll = 0.0F;
        break;
    case input::Key::End:
        scroll = maxScroll;
        break;
    default:
        break;
    }
}

// The remote of a TV also reaches the app as arrow keys and Enter, which move and press the focus while it shows.
void ErrorScreen::pressFocus(const platform::Event& event) {
    if (!isGamepadDriven() || event.repeat) {
        return;
    }
    if (event.key == input::Key::Left) {
        focused = Action::Copy;
    } else if (event.key == input::Key::Right) {
        focused = Action::Restart;
    } else if (focused == Action::Copy) {
        copyReport();
    } else {
        restartApp();
    }
}

// A press on a button runs its action, and a press anywhere else starts a drag that scrolls the content.
void ErrorScreen::press(math::Vec2 point) {
    if (copyButton.contains(point)) {
        copyReport();
        return;
    }
    if (restartButton.contains(point)) {
        restartApp();
        return;
    }
    dragging = Drag{.pointer = point.y, .scroll = scroll};
}

void ErrorScreen::drag(math::Vec2 point) {
    if (dragging) {
        scroll = std::clamp(dragging->scroll + dragging->pointer - point.y, 0.0F, maxScroll);
    }
}

// The first finger down presses and drags until it lifts, and the fingers that follow leave the screen alone.
void ErrorScreen::handleTouch(const platform::Event& event) {
    for (std::size_t index = 0; index < event.touchCount; ++index) {
        const platform::TouchPoint& touch = event.touches[index];
        if (event.type == platform::Event::Type::TouchBegan) {
            if (touch.changed && !finger) {
                finger = touch.id;
                press(toCanvas(touch.position));
            }
            continue;
        }
        if (touch.id != finger) {
            continue;
        }

        if (event.type == platform::Event::Type::TouchMoved) {
            drag(toCanvas(touch.position));
        } else if (touch.changed) {
            finger.reset();
            dragging.reset();
        }
    }
}

void ErrorScreen::update() {
    const input::Input& input = engine.getInput();
    const float line = kLineStep * getUnit();
    for (std::size_t index = 0; index < input::Input::kMaxGamepads; ++index) {
        if (input.isGamepadPressed(index, input::GamepadButton::DpadLeft)) {
            focused = Action::Copy;
        }
        if (input.isGamepadPressed(index, input::GamepadButton::DpadRight)) {
            focused = Action::Restart;
        }
        if (input.isGamepadPressed(index, input::GamepadButton::DpadUp)) {
            scrollBy(-line);
        }
        if (input.isGamepadPressed(index, input::GamepadButton::DpadDown)) {
            scrollBy(line);
        }
        if (input.isGamepadPressed(index, input::GamepadButton::South) && focused == Action::Copy) {
            copyReport();
        } else if (input.isGamepadPressed(index, input::GamepadButton::South)) {
            restartApp();
        }
    }
}

void ErrorScreen::handleEvent(const platform::Event& event) {
    switch (event.type) {
    case platform::Event::Type::KeyDown:
        handleKey(event);
        break;
    case platform::Event::Type::MouseScroll:
        scrollBy(-event.scroll.y * kLineStep * getUnit());
        break;
    case platform::Event::Type::MouseDown:
        if (event.mouseButton == input::MouseButton::Left) {
            press(toCanvas(event.position));
        }
        break;
    case platform::Event::Type::MouseMove:
        drag(toCanvas(event.position));
        break;
    case platform::Event::Type::MouseUp:
        dragging.reset();
        break;
    case platform::Event::Type::TouchBegan:
    case platform::Event::Type::TouchMoved:
    case platform::Event::Type::TouchEnded:
    case platform::Event::Type::TouchCancelled:
        handleTouch(event);
        break;
    default:
        break;
    }
}

float ErrorScreen::drawLine(graphics2d::Renderer& renderer, text::Font& font, std::string_view text, math::Vec2 position, const text::Style& style) const {
    renderer.drawText(font, text, position, style);
    return font.measure(text, style).y;
}

void ErrorScreen::render() {
    graphics2d::Renderer& renderer = engine.getRenderer2D();
    text::Font& font = *engine.getDefaultFont();
    const float unit = getUnit();
    const math::Rect safe = engine.getViewport().getSafeRect().inset(math::Insets::uniform(kMargin * unit));

    renderer.beginScreen();
    renderer.drawRect(renderer.getCanvasBounds(), kBackgroundColor);

    // The footer keeps its place at the bottom, and the content scrolls in the area above it.
    const float footerTop = drawFooter(renderer, font, safe, unit);
    const math::Rect content = math::Rect::fromMinMax(safe.getMin(), {safe.getRight(), std::max(safe.y, footerTop - kGap * unit)});
    page = content.height;
    scroll = std::clamp(scroll, 0.0F, maxScroll);

    renderer.pushClip(content);
    const float height = drawContent(renderer, font, {content.x, content.y - scroll}, content.width, unit);
    renderer.popClip();
    maxScroll = std::max(0.0F, height - content.height);
}

float ErrorScreen::drawContent(graphics2d::Renderer& renderer, text::Font& font, math::Vec2 origin, float width, float unit) const {
    const float gap = kGap * unit;
    float y = origin.y;
    y += drawLine(renderer, font, kTitle, {origin.x, y}, {.size = kTitleSize * unit, .color = kTitleColor}) + gap * 0.3F;
    y += drawLine(renderer, font, getDetails(), {origin.x, y}, {.size = kSmallSize * unit, .color = kMutedColor}) + gap;
    y += drawLine(renderer, font, expandTabs(error.getMessage()), {origin.x, y}, {.size = kMessageSize * unit, .color = kTextColor, .maxWidth = width}) + gap * 0.5F;

    if (!error.getFile().empty()) {
        y += drawLine(renderer, font, getLocation(), {origin.x, y}, {.size = kBodySize * unit, .color = kAccentColor}) + gap;
    }
    if (!excerpt.empty()) {
        y += drawExcerpt(renderer, font, {origin.x, y}, width, unit) + gap;
    }
    if (!error.getFrames().empty()) {
        y += drawStack(renderer, font, {origin.x, y}, unit);
    }
    return y - origin.y;
}

// Line numbers sit right-aligned in a column of their own, so the code starts at the same place on every line, and the error line is highlighted.
float ErrorScreen::drawExcerpt(graphics2d::Renderer& renderer, text::Font& font, math::Vec2 origin, float width, float unit) const {
    const text::Style code{.size = kBodySize * unit, .color = kCodeColor};
    const float padding = kPadding * unit;
    const float lineHeight = font.getLineHeight(code.size) * code.lineSpacing;
    const float numberWidth = font.measure(std::to_string(excerpt.back().number), code).x;
    const float codeX = origin.x + padding * 2.5F + numberWidth;
    const float height = lineHeight * static_cast<float>(excerpt.size()) + padding * 2.0F;
    renderer.drawRect({origin.x, origin.y, width, height}, kPanelColor);

    float y = origin.y + padding;
    for (const SourceLine& line : excerpt) {
        const bool failing = line.number == error.getLine();
        if (failing) {
            renderer.drawRect({origin.x, y, width, lineHeight}, kHighlightColor);
        }

        text::Style numberStyle = code;
        numberStyle.color = failing ? kTitleColor : kMutedColor;
        const std::string number = std::to_string(line.number);
        renderer.drawText(font, number, {origin.x + padding + numberWidth - font.measure(number, numberStyle).x, y}, numberStyle);
        renderer.drawText(font, line.text, {codeX, y}, code);
        y += lineHeight;
    }
    return height;
}

// The stack shows the location of every frame in one column and the function in the next, innermost first.
float ErrorScreen::drawStack(graphics2d::Renderer& renderer, text::Font& font, math::Vec2 origin, float unit) const {
    const text::Style heading{.size = kSmallSize * unit, .color = kMutedColor};
    const text::Style location{.size = kBodySize * unit, .color = kTextColor};
    const text::Style function{.size = kBodySize * unit, .color = kMutedColor};
    const float lineHeight = font.getLineHeight(location.size) * location.lineSpacing;

    float y = origin.y + drawLine(renderer, font, "Stack", origin, heading) + kGap * unit * 0.4F;
    float columnWidth = 0.0F;
    for (const lua::Error::Frame& frame : error.getFrames()) {
        columnWidth = std::max(columnWidth, font.measure(frame.getLocation(), location).x);
    }

    const float functionX = origin.x + columnWidth + kGap * unit;
    for (const lua::Error::Frame& frame : error.getFrames()) {
        renderer.drawText(font, frame.getLocation(), {origin.x, y}, location);
        renderer.drawText(font, frame.function, {functionX, y}, function);
        y += lineHeight;
    }
    return y - origin.y;
}

// Keyboards see the key of each action in front of its label, touch devices get plain buttons, and gamepads see the focused button outlined. Every button answers to a click or a tap.
math::Rect ErrorScreen::drawAction(graphics2d::Renderer& renderer, text::Font& font, math::Vec2 position, Action action, std::string_view key, std::string_view label, float unit) const {
    const text::Style style{.size = kBodySize * unit, .color = kTextColor};
    const float padding = kPadding * unit;
    const float height = kButtonHeight * unit;
    const math::Vec2 labelSize = font.measure(label, style);
    const bool gamepad = isGamepadDriven();
    const bool keyboard = !gamepad && !isTouchDevice();
    const float keyWidth = keyboard ? font.measure(key, style).x + padding * 1.6F : 0.0F;
    const math::Rect button{position.x, position.y, keyWidth + labelSize.x + padding * 2.0F, height};
    renderer.drawRect(button, kButtonColor);
    if (gamepad && action == focused) {
        renderer.drawRectOutline(button, std::max(2.0F, 2.0F * unit), kAccentColor);
    }

    float x = button.x + padding;
    const float textY = button.y + (height - labelSize.y) * 0.5F;
    if (keyboard) {
        const float keySize = height - padding;
        renderer.drawRect({x - padding * 0.4F, button.y + padding * 0.5F, keyWidth - padding * 0.4F, keySize}, kKeyColor);
        renderer.drawText(font, key, {x + padding * 0.2F, textY}, style);
        x += keyWidth;
    }
    renderer.drawText(font, label, {x, textY}, style);
    return button;
}

float ErrorScreen::drawFooter(graphics2d::Renderer& renderer, text::Font& font, const math::Rect& area, float unit) {
    const float gap = kGap * unit;
    const float buttonsTop = area.getBottom() - kButtonHeight * unit;
    copyButton = drawAction(renderer, font, {area.x, buttonsTop}, Action::Copy, "C", copied ? "Report copied" : "Copy report", unit);
    restartButton = drawAction(renderer, font, {copyButton.getRight() + gap, buttonsTop}, Action::Restart, "R", "Restart app", unit);

    float top = buttonsTop - gap;
    if (isReloadWatching()) {
        const text::Style hint{.size = kSmallSize * unit, .color = kMutedColor};
        const std::string_view text = "Saving a file of the app reloads it.";
        top -= font.measure(text, hint).y;
        renderer.drawText(font, text, {area.x, top}, hint);
        top -= gap * 0.6F;
    }

    renderer.drawLine({area.x, top}, {area.getRight(), top}, std::max(1.0F, unit), kButtonColor);
    return top;
}

} // namespace haylen::core
