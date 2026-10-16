#pragma once

#include <span>
#include <vector>

#include <imgui.h>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::ui {

// Decides which area a scroll gesture moves. A gesture of a wheel or a trackpad keeps the axis it starts along and goes to the innermost area under the pointer that can move along that axis the way it scrolls, or else to the innermost one that scrolls along it, and stays with that area until it pauses or the pointer moves on while it rests, wherever the content goes. Every area that scrolls registers while it draws, in any order, and a gesture chooses among the areas of the frame before, so its first scroll already moves the area it chose. A gesture that starts as the pointer arrives over an area, before the area knows the pointer is there, chooses again in the next frame, which receives the first scroll too. A finger that drags past the slop goes to the innermost area it pressed whose axis the drag follows, from the frame after the areas it pressed offered themselves, and stays with it until it lifts.
class ScrollArbiter final {
  public:
    // Steps of a wheel and points of the screen that a trackpad moved, with positive values for scrolling up and to the left.
    struct Wheel {
        math::Vec2 lines{};
        math::Vec2 points{};
    };

    // The part of a wheel that moves one area along its axis, with positive values toward its top or left end.
    struct AxisWheel {
        float lines = 0.0F;
        float points = 0.0F;
    };

    // How far a finger travels, in UI units, before its drag scrolls instead of pressing.
    static constexpr float kDragSlop = 12.0F;

    void addWheel(math::Vec2 scroll, bool precise) noexcept;

    // Starts a frame once ImGui has read the pointer of the frame, and settles the gestures from the areas of the frame before.
    void beginFrame(float seconds);

    // The wheel of the frame as it reached the UI, which node events report.
    [[nodiscard]] const Wheel& getWheel() const noexcept {
        return wheel;
    }

    // Returns the wheel of the frame for an area that scrolls along one axis, and nothing while the gesture belongs to another area. The `position` is how far the area shows from its top or left end, out of `maximum`.
    [[nodiscard]] AxisWheel takeWheel(ImGuiID area, const math::Rect& bounds, bool horizontal, double position, double maximum, bool hovered);

    // Returns whether the drag of the finger belongs to an area that can scroll along `horizontal`.
    [[nodiscard]] bool takeDrag(ImGuiID area, const math::Rect& bounds, bool horizontal);

  private:
    // An area that scrolls by the wheel, as it registered in a frame.
    struct Area {
        ImGuiID id = 0;
        math::Rect bounds;
        bool horizontal = false;
        double position = 0.0;
        double maximum = 0.0;
        bool hovered = false;
    };

    struct Offer {
        ImGuiID area = 0;
        float size = 0.0F;
        bool movable = false;
    };

    // A gesture ends once the wheel rests this long, or sooner once the pointer traveled away from where it last scrolled.
    static constexpr float kGesturePause = 0.35F;
    static constexpr float kPointerPause = 0.1F;
    static constexpr float kPointerTravel = 4.0F;

    // Returns the smallest area that offered, among those that can move when any can, so an area inside another wins over it.
    [[nodiscard]] static ImGuiID choose(std::span<const Offer> offers) noexcept;

    // Chooses the area of a gesture among the areas of the frame before.
    void chooseArea(math::Vec2 pointer);
    [[nodiscard]] static bool isEmpty(const Wheel& value) noexcept;

    Wheel pending;
    Wheel wheel;
    Wheel delivered;
    Wheel waiting;
    bool gesture = false;
    bool vertical = true;
    ImGuiID wheelArea = 0;
    float quiet = 0.0F;
    math::Vec2 wheelPointer;
    std::vector<Area> areas;
    std::vector<Area> previousAreas;
    std::vector<Offer> wheelOffers;
    ImGuiID dragArea = 0;
    std::vector<Offer> dragOffers;
};

} // namespace haylen::ui
