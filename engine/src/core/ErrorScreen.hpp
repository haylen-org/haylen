#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/lua/Error.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::platform {
struct Event;
}

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::text {
class Font;
struct Style;
} // namespace haylen::text

namespace haylen::core {

class Engine;

// What the engine draws in place of the app once a script error stops it: the app and engine it ran on, the message, a source excerpt, the stack and the actions. Content taller than the screen scrolls with the mouse wheel, a drag, the arrow keys and the directional pad. C copies the report and R restarts the app, and both actions are buttons too, the only way to reach them on touch devices. Gamepads and TV remotes move the focus between the buttons with the directional pad, or the arrow keys that TV remotes also send, and press the focused one with the south button or Enter, and the focus starts on Restart. It draws with the renderer alone, so it keeps working when the UI is what failed.
class ErrorScreen final {
  public:
    enum class Action { Copy, Restart };

    struct SourceLine {
        int number = 0;
        std::string text;
    };

    static constexpr std::string_view kTitle = "The app stopped with an error";

    ErrorScreen(Engine& owner, lua::Error failure);

    [[nodiscard]] const lua::Error& getError() const noexcept {
        return error;
    }

    // The lines around the error line when the error points into a file of the app package, with tabs expanded to spaces.
    [[nodiscard]] const std::vector<SourceLine>& getExcerpt() const noexcept {
        return excerpt;
    }

    // The whole error as plain text, the way the log and the clipboard receive it.
    [[nodiscard]] const std::string& getReport() const noexcept {
        return report;
    }

    // The content scrolls from 0 at its top to the maximum the last frame measured, in design units like the buttons.
    [[nodiscard]] float getScroll() const noexcept {
        return scroll;
    }
    [[nodiscard]] float getMaxScroll() const noexcept {
        return maxScroll;
    }
    [[nodiscard]] const math::Rect& getCopyButton() const noexcept {
        return copyButton;
    }
    [[nodiscard]] const math::Rect& getRestartButton() const noexcept {
        return restartButton;
    }
    [[nodiscard]] Action getFocusedAction() const noexcept {
        return focused;
    }

    void handleEvent(const platform::Event& event);
    // Reads the gamepads once per frame, since their buttons arrive as state rather than as events.
    void update();
    void render();

    void copyReport();
    void restartApp();

  private:
    struct Drag {
        float pointer = 0.0F;
        float scroll = 0.0F;
    };

    static constexpr int kExcerptRadius = 3;
    static constexpr std::size_t kTabWidth = 4;

    // Sizes are in points, which the screen turns into design units, so text keeps its physical size whatever the design resolution of the app.
    static constexpr float kMargin = 28.0F;
    static constexpr float kGap = 18.0F;
    static constexpr float kPadding = 12.0F;
    static constexpr float kTitleSize = 26.0F;
    static constexpr float kMessageSize = 19.0F;
    static constexpr float kBodySize = 15.0F;
    static constexpr float kSmallSize = 13.0F;
    static constexpr float kButtonHeight = 42.0F;
    static constexpr float kLineStep = 40.0F;

    static constexpr math::Color kBackgroundColor = math::Color::fromHex(0x161925FFU);
    static constexpr math::Color kPanelColor = math::Color::fromHex(0x202432FFU);
    static constexpr math::Color kHighlightColor = math::Color::fromHex(0xFF6B6B38U);
    static constexpr math::Color kTitleColor = math::Color::fromHex(0xFF6B6BFFU);
    static constexpr math::Color kTextColor = math::Color::fromHex(0xECEEF4FFU);
    static constexpr math::Color kCodeColor = math::Color::fromHex(0xC9CFDCFFU);
    static constexpr math::Color kMutedColor = math::Color::fromHex(0x8D96ABFFU);
    static constexpr math::Color kAccentColor = math::Color::fromHex(0xF5C26BFFU);
    static constexpr math::Color kButtonColor = math::Color::fromHex(0x2E3447FFU);
    static constexpr math::Color kKeyColor = math::Color::fromHex(0x454D66FFU);

    [[nodiscard]] static std::string expandTabs(std::string_view text);
    [[nodiscard]] std::vector<SourceLine> readExcerpt() const;
    [[nodiscard]] std::string getDetails() const;
    [[nodiscard]] std::string getLocation() const;
    [[nodiscard]] std::string buildReport() const;
    [[nodiscard]] bool isTouchDevice() const;
    [[nodiscard]] bool isGamepadDriven() const;
    [[nodiscard]] bool isReloadWatching() const;

    // Design units per point, from the pixel density of the window and the scale of the viewport.
    [[nodiscard]] float getUnit() const;
    [[nodiscard]] math::Vec2 toCanvas(math::Vec2 framebufferPoint) const;

    void handleKey(const platform::Event& event);
    void pressFocus(const platform::Event& event);
    void handleTouch(const platform::Event& event);
    void press(math::Vec2 point);
    void drag(math::Vec2 point);
    void scrollBy(float amount);

    float drawLine(graphics2d::Renderer& renderer, text::Font& font, std::string_view text, math::Vec2 position, const text::Style& style) const;
    [[nodiscard]] float drawContent(graphics2d::Renderer& renderer, text::Font& font, math::Vec2 origin, float width, float unit) const;
    [[nodiscard]] float drawExcerpt(graphics2d::Renderer& renderer, text::Font& font, math::Vec2 origin, float width, float unit) const;
    [[nodiscard]] float drawStack(graphics2d::Renderer& renderer, text::Font& font, math::Vec2 origin, float unit) const;
    [[nodiscard]] float drawFooter(graphics2d::Renderer& renderer, text::Font& font, const math::Rect& area, float unit);
    [[nodiscard]] math::Rect drawAction(graphics2d::Renderer& renderer, text::Font& font, math::Vec2 position, Action action, std::string_view key, std::string_view label, float unit) const;

    Engine& engine;
    lua::Error error;
    std::vector<SourceLine> excerpt;
    std::string report;
    float scroll = 0.0F;
    float maxScroll = 0.0F;
    float page = 0.0F;
    std::optional<Drag> dragging;
    std::optional<std::uint64_t> finger;
    math::Rect copyButton{};
    math::Rect restartButton{};
    Action focused = Action::Restart;
    bool copied = false;
};

} // namespace haylen::core
