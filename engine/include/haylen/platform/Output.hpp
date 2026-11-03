#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::platform {

// What the app shows on a display other than the one of its window: a window of its own that covers the display or sits on it on a desktop, the scene that UIKit connects for an external display of an iPhone or an iPad, or a window of the browser on another screen. The app draws into its render target with any canvas, and the engine shows the target on the display at the end of every frame, as large as it fits, with bars of the clear color around it. The target takes the size of the display in pixels and changes its size with it. An output opens until the platform gives it its surface, shows while the display shows it, hides while the platform holds its surface back, such as while the app is in the background, and closes once the app closed it, the person closed its window, its display disconnected or the platform could not show it there.
class Output final {
  public:
    enum class State : std::uint8_t {
        Opening,
        Shown,
        Hidden,
        Closed,
    };

    enum class Ending : std::uint8_t {
        App,
        Person,
        Disconnected,
        Unsupported,
    };

    static constexpr std::array<std::pair<std::string_view, State>, 4> kStateNames{{{"opening", State::Opening}, {"shown", State::Shown}, {"hidden", State::Hidden}, {"closed", State::Closed}}};
    static constexpr std::array<std::pair<std::string_view, Ending>, 4> kEndingNames{{{"app", Ending::App}, {"person", Ending::Person}, {"disconnected", Ending::Disconnected}, {"unsupported", Ending::Unsupported}}};

    // How an output opens: on a desktop, a window that covers the display or a window of the size in points with the title, and the color of the bars around the target.
    struct Options {
        bool fullscreen = true;
        math::Vec2 size{1280.0F, 720.0F};
        std::string title;
        math::Color clearColor = math::Color::black();
    };

    Output(std::uint32_t number, std::string display, math::Color clear) : id(number), displayId(std::move(display)), clearColor(clear) {}

    [[nodiscard]] std::uint32_t getId() const noexcept {
        return id;
    }

    [[nodiscard]] const std::string& getDisplayId() const noexcept {
        return displayId;
    }

    [[nodiscard]] State getState() const noexcept {
        return state;
    }

    // Why the output closed, which matters only once it is closed.
    [[nodiscard]] Ending getEnding() const noexcept {
        return ending;
    }

    // The render target that the app draws into, which is invalid until the output first shows and changes when the display changes its size.
    [[nodiscard]] const graphics::RenderTarget& getTarget() const noexcept {
        return target;
    }

    // The pixels in a point of the display.
    [[nodiscard]] float getScale() const noexcept {
        return scale;
    }

    [[nodiscard]] math::Color getClearColor() const noexcept {
        return clearColor;
    }

    void setClearColor(math::Color value) noexcept {
        clearColor = value;
    }

    // The surface of the GPU device that the target shows in, or 0 while the output does not show.
    [[nodiscard]] int getSurface() const noexcept {
        return surface;
    }

    [[nodiscard]] math::Vec2 getSurfaceSize() const noexcept {
        return surfaceSize;
    }

  private:
    friend class Outputs;

    std::uint32_t id;
    std::string displayId;
    State state = State::Opening;
    Ending ending = Ending::App;
    graphics::RenderTarget target;
    float scale = 1.0F;
    math::Color clearColor;
    int surface = 0;
    math::Vec2 surfaceSize;
};

} // namespace haylen::platform
