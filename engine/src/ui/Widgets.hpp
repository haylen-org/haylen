#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

#include <imgui.h>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/FocusDirection.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::ui {

class Context;

// The one implementation of every control the components draw, so each kind looks the same wherever it appears. Positions are UI coordinates, which are ImGui screen coordinates.
class Widgets final {
  public:
    enum class ButtonVariant : std::uint8_t {
        Default,
        Primary,
        Destructive,
        Toolbar,
        Icon,
        Link,
    };

    enum class Tone : std::uint8_t {
        Neutral,
        Accent,
        Success,
        Warning,
        Danger,
        Information,
    };

    struct Interaction {
        bool hovered = false;
        bool held = false;
        bool clicked = false;
    };

    struct ToneColors {
        Theme::Color fill;
        Theme::Color ink;
        Theme::Color background;
        Theme::Color text;
    };

    static constexpr std::array<std::pair<std::string_view, ButtonVariant>, 6> kButtonVariants{{
        {"default", ButtonVariant::Default},
        {"primary", ButtonVariant::Primary},
        {"destructive", ButtonVariant::Destructive},
        {"toolbar", ButtonVariant::Toolbar},
        {"icon", ButtonVariant::Icon},
        {"link", ButtonVariant::Link},
    }};

    static constexpr std::array<std::pair<std::string_view, Tone>, 6> kTones{{
        {"neutral", Tone::Neutral},
        {"accent", Tone::Accent},
        {"success", Tone::Success},
        {"warning", Tone::Warning},
        {"danger", Tone::Danger},
        {"information", Tone::Information},
    }};

    // The largest magnitude of the ends of a slider, which keeps its span a finite number.
    static constexpr double kSliderLimit = 1e15;

    [[nodiscard]] static ToneColors getToneColors(Tone tone) noexcept;

    // Registers an interactive item for mouse and touch and as a focus target, and draws the focus ring around it while it has the visible focus. The radius is the corner radius of the control. Items outside the visible clip report nothing.
    [[nodiscard]] static Interaction interact(Context& context, const math::Rect& bounds, float radius, std::string_view label = "##item", ImGuiButtonFlags flags = ImGuiButtonFlags_None);

    // Whether any part of the bounds shows inside the clip of the window being drawn, where the transforms of the nodes around them move and scale them, so long collections skip drawing what is scrolled out of view.
    [[nodiscard]] static bool isVisible(const Context& context, const math::Rect& bounds);

    // Draws the themed focus ring a small gap outside the bounds while the item has the visible focus, with corners concentric to the corners of the control.
    static void drawFocusRing(Context& context, const math::Rect& bounds, ImGuiID id, float radius);

    // Gives the focus to the last item, which lets a menu start with its first button selected. The ring keeps its visibility, and shows at once when the player last used a gamepad or has no pointer device.
    static void focusItem(Context& context);

    [[nodiscard]] static math::Vec2 measureButton(Context& context, std::string_view label, bool hasIcon, ButtonVariant variant);
    [[nodiscard]] static bool button(Context& context, const math::Rect& bounds, std::string_view label, const graphics::Texture* icon, ButtonVariant variant, bool checked = false);

    [[nodiscard]] static math::Vec2 measureChoice(Context& context, std::string_view label);
    static bool checkbox(Context& context, const math::Rect& bounds, bool& value, std::string_view label);
    static bool radio(Context& context, const math::Rect& bounds, bool selected, std::string_view label, std::string_view idLabel);

    [[nodiscard]] static math::Vec2 measureToggle(Context& context, std::string_view label);
    static bool toggle(Context& context, const math::Rect& bounds, bool& value, std::string_view label);

    static bool slider(Context& context, const math::Rect& bounds, double& value, double minimum, double maximum, double step);

    // Returns the value inside the range, on the nearest multiple of the step from the minimum when the step is above zero.
    [[nodiscard]] static double snap(double value, double minimum, double maximum, double step) noexcept;
    static void progress(Context& context, const math::Rect& bounds, float value, Tone tone);
    static void spinner(Context& context, math::Vec2 center, float radius, math::Color color);

    // Draws a filled triangle pointing in a direction, sized in design units and centered on a point.
    static void arrow(math::Vec2 center, float size, ImGuiDir direction, math::Color color);

    // Returns the direction an arrow points in the direction of the UI, which swaps left and right in a right-to-left UI, so an arrow that means forward points the way the UI reads.
    [[nodiscard]] static ImGuiDir mirror(const Context& context, ImGuiDir direction) noexcept;

    // Returns the step a horizontal direction the player pressed takes along a control that grows toward the end of the UI: one forward when it points toward the end and one back otherwise.
    [[nodiscard]] static int getStep(const Context& context, FocusDirection direction) noexcept;

    // Places a popup just below an anchor, as wide as the anchor unless its content needs more, lined up with the side of the anchor where the UI starts.
    static void placePopup(const Context& context, const math::Rect& anchor, float width);

  private:
    struct SurfaceSet {
        Theme::Surface normal;
        Theme::Surface hover;
        Theme::Surface pressed;
    };

    static constexpr float kContentSpacing = 12.0F;
    static constexpr float kFocusGap = 3.0F;

    // Paints overlay on top of base the way a translucent layer would.
    [[nodiscard]] static math::Color mix(math::Color base, math::Color overlay) noexcept;
    [[nodiscard]] static SurfaceSet getSurfaces(ButtonVariant variant) noexcept;

    // A theme may leave out the hover or pressed image of a surface, and then the normal image stands in for it.
    [[nodiscard]] static const Theme::Image* getStateImage(Context& context, const SurfaceSet& set, const Interaction& state);
};

} // namespace haylen::ui
