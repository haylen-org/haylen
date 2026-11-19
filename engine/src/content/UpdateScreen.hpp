#pragma once

#include <string>
#include <string_view>

#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::core {
class Engine;
}

namespace haylen::platform {
struct Event;
}

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::text {
class Font;
}

namespace haylen::content {

class ContentManager;

// What the engine draws while an app waits for the content it needs to start, in the background color of its splash screen with the name of the app: the progress of the download with its bytes, and after a failure its reason, such as an offline device or a full disk, with a button that tries again and, where the platform lets apps close themselves, one that quits. A mouse, a finger, the keyboard, a gamepad and a TV remote press the buttons, and the directional pad and the arrow keys move the focus between them. It draws no logo, since the launch screen of each platform shows the logo at a place of its own and fades out into this screen, so no logo moves as one hands over to the other.
class UpdateScreen final {
  public:
    enum class Action { Retry, Quit };

    explicit UpdateScreen(core::Engine& owner);

    void handleEvent(const platform::Event& event);

    // Reads the gamepads once per frame, since their buttons arrive as state rather than as events.
    void update();
    void render();

    [[nodiscard]] Action getFocusedAction() const noexcept {
        return focused;
    }

    // Whether the platform lets an app close itself, which iOS, iPadOS and tvOS do not.
    [[nodiscard]] bool canQuit() const;

    // The sentence under the progress bar: what downloads, or why the content could not come.
    [[nodiscard]] std::string getStatus() const;

    // The bytes in the units people read, such as `12.4 MB`.
    [[nodiscard]] static std::string formatBytes(std::uint64_t bytes);

  private:
    static constexpr float kMargin = 32.0F;
    static constexpr float kGap = 18.0F;
    static constexpr float kTitleSize = 30.0F;
    static constexpr float kBodySize = 18.0F;
    static constexpr float kButtonHeight = 46.0F;
    static constexpr float kButtonRadius = 12.0F;
    static constexpr float kBarHeight = 10.0F;
    static constexpr float kBarWidth = 520.0F;

    static constexpr math::Color kTrackColor = math::Color::fromHex(0xFFFFFF33U);
    static constexpr math::Color kBarColor = math::Color::fromHex(0xF5C26BFFU);
    static constexpr math::Color kButtonColor = math::Color::fromHex(0x2E3447E6U);
    static constexpr math::Color kErrorColor = math::Color::fromHex(0xFF8A80FFU);

    [[nodiscard]] float getUnit() const;
    [[nodiscard]] math::Color getTextColor() const;
    [[nodiscard]] bool hasFailed() const;
    void run(Action action);
    void press(math::Vec2 point);
    void moveFocus();
    [[nodiscard]] math::Rect drawButton(graphics2d::Renderer& renderer, text::Font& font, math::Vec2 center, std::string_view label, Action action, float unit) const;

    core::Engine& engine;
    ContentManager& content;
    Action focused = Action::Retry;
    math::Rect retryButton{};
    math::Rect quitButton{};
};

} // namespace haylen::content
