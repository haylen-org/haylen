#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/ui/NavigationTransition.hpp"
#include "haylen/ui/Region.hpp"
#include "haylen/ui/Scaling.hpp"

namespace haylen::plugins {
class UiPlugin;
}

namespace haylen::ui {

class Navigation;
class PresentationView;
class Screen;

// A screen presented over what the app shows, which starts a flow of its own, such as a new post, a checkout or a picker. A modal presentation takes the input and the focus of everything under it until it leaves, and a sheet that is not modal leaves them to the app, such as the queue of a player that stays open while the person browses. The presented flow may return a value to whoever presented it, which the presentation keeps as its result once it leaves. A sheet rises from the bottom on a surface of the theme panel color. On a compact screen it fills the width and rests at one of its detents, such as half the screen or nearly all of it, which a drag on its grabber or its empty surface moves between, and a drag below its lowest detent dismisses it. On a wide screen a form sheet floats as a card in the middle and a page sheet rests at its detents along the bottom with a bounded width. A full screen presentation covers the whole screen, a dialog fades in over the dimmed screen and draws its own card, a drawer slides in from the start edge, and a popover floats next to the control it points at with an arrow, or becomes a sheet on a compact screen.
class Presentation final : public std::enable_shared_from_this<Presentation> {
  public:
    enum class Style : std::uint8_t {
        Sheet,
        FullScreen,
        Dialog,
        Drawer,
        Popover,
    };

    // How a sheet shows on a wide screen: a card in the middle, or a page that rests at its detents along the bottom.
    enum class Size : std::uint8_t {
        Form,
        Page,
    };

    // A height a sheet rests at on a compact screen and a page sheet on a wide one: about half the screen, nearly all of it, the height its screen measures, or a height in UI units above the bottom of the safe area.
    struct Detent {
        enum class Kind : std::uint8_t {
            Medium,
            Large,
            Fit,
            Height,
        };

        Kind kind = Kind::Large;
        float height = 0.0F;
    };

    // The widest and tallest form sheet of a wide screen, the widest page sheet, the room a sheet keeps from the edges of the safe area and above it on a compact screen, the width of a popover and a drawer, and the size of the arrow of a popover, in UI units.
    static constexpr float kSheetWidth = 760.0F;
    static constexpr float kSheetHeight = 1000.0F;
    static constexpr float kPageWidth = 1040.0F;
    static constexpr float kSheetMargin = 32.0F;
    static constexpr float kSheetTop = 24.0F;
    static constexpr float kPopoverWidth = 480.0F;
    static constexpr float kDrawerWidth = 560.0F;
    static constexpr float kArrowSize = 20.0F;
    static constexpr float kDialogDuration = 0.2F;

    // The band along the top of a sheet that holds its grabber, and the size of the grabber, in UI units.
    static constexpr float kGrabberBand = 36.0F;
    static constexpr math::Vec2 kGrabberSize{72.0F, 10.0F};

    // How much the GUIs behind a page sheet at its largest detent shrink on a compact screen, so the sheet reads as a card over them.
    static constexpr float kBehindScale = 0.92F;

    struct Options {
        Style style = Style::Sheet;

        // The transition of the presentation and of its dismissal: a rise for a sheet and a full screen presentation, a slide for a drawer and a fade of `kDialogDuration` for a dialog and a popover when empty.
        std::optional<NavigationTransition> transition;

        // Whether the person dismisses it with a tap on the backdrop, a drag, Escape or the system back.
        bool dismissible = true;

        // Whether it takes the input and the focus of everything under it, which only a sheet may give up.
        bool modal = true;

        // The color of the surface in place of the theme color, such as a dark stage behind pictures.
        std::optional<math::Color> color;

        // The widest and tallest form sheet of a wide screen, in UI units.
        float maxWidth = kSheetWidth;
        float maxHeight = kSheetHeight;
        Size size = Size::Form;

        // The detents of a sheet from the lowest to the highest, and the one it opens at.
        std::vector<Detent> detents{Detent{}};
        std::size_t detent = 0;

        // Whether a sheet shows its grabber, which it does by default while it has more than one detent.
        std::optional<bool> grabber;

        // The width of a popover and of a drawer, in UI units.
        std::optional<float> width;

        // The node that a popover points at.
        std::optional<Region> anchor;
    };

    // Where a presentation lays out its screen and draws its surface in UI coordinates, the radius of each corner of the surface from the top left clockwise, the grabber of a sheet, the tip of the arrow of a popover and the side it points from, and how much the GUIs behind it shrink.
    struct Layout {
        math::Rect area;
        math::Rect surface;
        std::array<float, 4> radii{};
        math::Rect grabber;
        std::optional<math::Vec2> arrow;
        bool arrowUp = false;
        bool compact = false;
        float behindScale = 1.0F;
    };

    // What a layout depends on besides the presentation: the visible and safe areas of the screen, the points of the screen one UI unit spans, the corner radius of the theme panels, the height the screen measures at the width of its area, where the anchor of a popover drew, and the direction of the UI.
    struct Frame {
        math::Rect display;
        math::Rect safe;
        float pointsPerUnit = Scaling::kPointsPerUnit;
        float radius = 0.0F;
        float contentHeight = 0.0F;
        std::optional<math::Rect> anchor;
        bool rightToLeft = false;
    };

    // Navigation creates presentations, which register with it. Throws `std::invalid_argument` for a missing screen, a screen that a stack or another presentation holds, a size that is not positive, no detents, a detent out of the list, a sheet that is not modal and is not a sheet, and a popover without an anchor.
    Presentation(Navigation& owner, std::shared_ptr<Screen> presented, Options settings);
    ~Presentation();

    Presentation(const Presentation&) = delete;
    Presentation& operator=(const Presentation&) = delete;

    // Plays the transition back and removes the screen once the presentation left, with the result that whoever presented it receives. A presentation that leaves already stays as it is.
    void dismiss(core::Json value = nullptr, const std::optional<NavigationTransition>& transition = std::nullopt);

    // Moves a sheet to one of its detents with a glide, and throws `std::invalid_argument` for an index out of its list.
    void setDetent(std::size_t index);
    [[nodiscard]] std::size_t getDetent() const noexcept {
        return detent;
    }

    [[nodiscard]] const std::shared_ptr<Screen>& getScreen() const noexcept {
        return screen;
    }
    [[nodiscard]] Style getStyle() const noexcept {
        return options.style;
    }
    [[nodiscard]] const Options& getOptions() const noexcept {
        return options;
    }
    [[nodiscard]] bool isDismissible() const noexcept {
        return options.dismissible;
    }
    [[nodiscard]] bool isModal() const noexcept {
        return options.modal;
    }

    // Whether the presentation leaves or left already.
    [[nodiscard]] bool isDismissed() const noexcept {
        return phase == Phase::Leaving || phase == Phase::Gone;
    }

    // The value the presentation was dismissed with, null until then and when the person dismissed it.
    [[nodiscard]] const core::Json& getResult() const noexcept {
        return result;
    }

    // Returns the width of the area the screen lays out in, which the height its screen measures at that width follows.
    [[nodiscard]] float getAreaWidth(const Frame& frame) const;

    // Returns where the screen lays out and the surface draws, at the detent the sheet rests at, moved by a drag or a glide between detents.
    [[nodiscard]] Layout getLayout(const Frame& frame) const;

    [[nodiscard]] static std::optional<Style> styleFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view styleName(Style value) noexcept;

    static constexpr std::array<std::pair<std::string_view, Style>, 5> kStyleNames{{{"sheet", Style::Sheet}, {"fullScreen", Style::FullScreen}, {"dialog", Style::Dialog}, {"drawer", Style::Drawer}, {"popover", Style::Popover}}};
    static constexpr std::array<std::pair<std::string_view, Size>, 2> kSizeNames{{{"form", Size::Form}, {"page", Size::Page}}};
    static constexpr std::array<std::pair<std::string_view, Detent::Kind>, 3> kDetentNames{{{"medium", Detent::Kind::Medium}, {"large", Detent::Kind::Large}, {"fit", Detent::Kind::Fit}}};

    // Tells that a sheet rests at another detent, after a drag or `setDetent`.
    core::Signal<Presentation&> detentChanged;

    // Tells that the presentation left the screen and its screen was removed, once its result is known.
    core::Signal<Presentation&> dismissed;

  private:
    friend class Navigation;
    friend class PresentationView;
    friend class plugins::UiPlugin;

    enum class Phase : std::uint8_t {
        Arriving,
        Shown,
        Leaving,
        Gone,
    };

    // How fast the offset of a sheet or a drawer glides back to its resting place, per second, and how far a sheet stretches past its highest detent compared with the finger.
    static constexpr float kGlideSpeed = 14.0F;
    static constexpr float kStretch = 0.3F;

    // The speed of a drag that flings the surface away whatever its distance, in UI units per second, and how far ahead of where a sheet is its speed carries it, in seconds.
    static constexpr float kFlingSpeed = 1800.0F;
    static constexpr float kCarrySeconds = 0.2F;

    // Opens the presentation: its screen joins, builds its GUI and appears.
    void open();
    void advance(float deltaSeconds);
    void finishArriving();
    void finishLeaving();

    // How far the presentation is from showing whole, from 1 before it arrives to 0 once it shows.
    [[nodiscard]] float getDistance() const;

    // Whether the presentation shows as a sheet: a sheet, or a popover on a compact screen.
    [[nodiscard]] bool isSheet(const Frame& frame) const;

    // Whether the presentation shows as a card in the middle: a form sheet on a wide screen.
    [[nodiscard]] bool isCard(const Frame& frame) const;
    [[nodiscard]] bool isCompact(const Frame& frame) const;

    // The top of the surface of a sheet resting at a detent.
    [[nodiscard]] float getDetentTop(const Frame& frame, std::size_t index) const;
    [[nodiscard]] Layout getSheetLayout(const Frame& frame) const;
    [[nodiscard]] float getCardHeight(const Frame& frame) const;
    [[nodiscard]] Layout getDrawerLayout(const Frame& frame) const;
    [[nodiscard]] Layout getPopoverLayout(const Frame& frame) const;

    // The person drags the surface by a distance toward where it closes, and lets go of it at a speed in UI units per second, which rests a sheet at the detent nearest to where it is heading or dismisses the presentation past its lowest detent.
    void drag(float distance, float speed);
    void release();

    // Asks the screen whether the person may take it away, and dismisses the presentation when it may, returning whether it did.
    [[nodiscard]] bool dismissByPerson();

    Navigation& navigation;
    std::shared_ptr<Screen> screen;
    Options options;
    NavigationTransition transition;
    Phase phase = Phase::Arriving;
    float elapsed = 0.0F;
    core::Json result;
    std::size_t detent = 0;

    // How far the surface lies from its resting place toward where it closes, which a drag sets and a glide brings back to zero, and the speed of the drag.
    float offset = 0.0F;
    float speed = 0.0F;
    float dragStart = 0.0F;
    bool dragging = false;

    // What the presentation laid out against when it last drew, which a glide between detents and the end of a drag measure from.
    std::optional<Frame> lastFrame;

    // How long the presentation waited for its screen to lay out with every picture it shows before it started to arrive.
    float waited = 0.0F;
    bool moving = false;
};

} // namespace haylen::ui
