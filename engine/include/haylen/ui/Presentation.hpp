#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/core/Signal.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/ui/NavigationTransition.hpp"

namespace haylen::ui {

class Navigation;
class Screen;

// A screen presented over everything the app shows, which starts a flow of its own, such as a new post or a checkout. Its screen may hold its own bars and stacks, and the GUIs below it take no input or focus until it leaves. A sheet rises on a surface of the theme panel color that fills the width of a narrow screen up to the bottom edge, or floats as a rounded card in the middle of a wide one, over a dimmed backdrop. A full screen presentation covers the whole screen with the theme background color, and a dialog lays its screen out over the dimmed backdrop in the safe area, where the screen draws its own card. A tap on the backdrop dismisses a dismissible sheet or dialog, and so does the system back once the stacks of the presentation are at their root.
class Presentation final : public std::enable_shared_from_this<Presentation> {
  public:
    enum class Style : std::uint8_t {
        Sheet,
        FullScreen,
        Dialog,
    };

    // The widest and tallest sheet of a wide screen, the room it keeps from the edges of the safe area, and the room above a sheet on a narrow screen, in UI units.
    static constexpr float kSheetWidth = 760.0F;
    static constexpr float kSheetHeight = 1000.0F;
    static constexpr float kSheetMargin = 32.0F;
    static constexpr float kSheetTop = 24.0F;
    static constexpr float kDialogDuration = 0.2F;

    struct Options {
        Style style = Style::Sheet;

        // The transition of the presentation and of its dismissal, a rise for a sheet and a full screen and a fade for a dialog when empty.
        std::optional<NavigationTransition> transition;

        // Whether a tap on the backdrop and the system back dismiss it.
        bool dismissible = true;

        // The color of the surface of a sheet or a full screen presentation in place of the theme color, such as a dark stage behind pictures.
        std::optional<math::Color> color;

        // The widest and tallest that a sheet grows on a wide screen, in UI units.
        float maxWidth = kSheetWidth;
        float maxHeight = kSheetHeight;
    };

    // Where a presentation lays out its screen and draws its surface, in UI coordinates, and the radius of each corner of the surface, from the top left clockwise.
    struct Layout {
        math::Rect area;
        math::Rect surface;
        std::array<float, 4> radii{};
    };

    // Navigation creates presentations, which register with it. Throws `std::invalid_argument` for a missing screen, a screen that a stack or another presentation holds and a width or height that is not positive.
    Presentation(Navigation& owner, std::shared_ptr<Screen> presented, Options settings);
    ~Presentation();

    Presentation(const Presentation&) = delete;
    Presentation& operator=(const Presentation&) = delete;

    // Plays the transition back and removes the screen once the presentation left. A presentation that leaves already stays as it is.
    void dismiss(const std::optional<NavigationTransition>& transition = std::nullopt);

    [[nodiscard]] const std::shared_ptr<Screen>& getScreen() const noexcept {
        return screen;
    }
    [[nodiscard]] Style getStyle() const noexcept {
        return options.style;
    }
    [[nodiscard]] bool isDismissible() const noexcept {
        return options.dismissible;
    }
    [[nodiscard]] const std::optional<math::Color>& getColor() const noexcept {
        return options.color;
    }

    // Whether the presentation leaves or left already.
    [[nodiscard]] bool isDismissed() const noexcept {
        return phase == Phase::Leaving || phase == Phase::Gone;
    }

    // Returns where the screen lays out on a screen with the visible and safe areas and the corner radius of the theme panels.
    [[nodiscard]] Layout getLayout(const math::Rect& display, const math::Rect& safe, float radius) const;

    [[nodiscard]] static std::optional<Style> styleFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view styleName(Style value) noexcept;

    static constexpr std::array<std::pair<std::string_view, Style>, 3> kStyleNames{{{"sheet", Style::Sheet}, {"fullScreen", Style::FullScreen}, {"dialog", Style::Dialog}}};

    // Tells that the presentation left the screen and its screen was removed.
    core::Signal<Presentation&> dismissed;

  private:
    friend class Navigation;

    enum class Phase : std::uint8_t {
        Arriving,
        Shown,
        Leaving,
        Gone,
    };

    // Opens the presentation: its screen joins, builds its GUI and appears.
    void open();
    void advance(float deltaSeconds);
    void finishArriving();
    void finishLeaving();

    // How far the presentation is from showing whole, from 1 before it arrives to 0 once it shows.
    [[nodiscard]] float getDistance() const;

    Navigation& navigation;
    std::shared_ptr<Screen> screen;
    Options options;
    NavigationTransition transition;
    Phase phase = Phase::Arriving;
    float elapsed = 0.0F;
};

} // namespace haylen::ui
