#pragma once

#include <imgui.h>

#include <memory>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/Presentation.hpp"

namespace haylen::ui {

class Backend;
class Context;
class Gui;

// Draws what a presentation shows around its screen and answers the gestures on it: the backdrop that dims what lies under a modal presentation, the surface with its shadow, the arrow of a popover and the grabber of a sheet, a drag that moves a sheet between its detents or a drawer and a card out, and a tap on the backdrop. What the person asked for that runs code of the app, such as a dismissal or another detent, waits for the next update, which takes it.
class PresentationView final {
  public:
    // What the person asked of the presentations while they drew, which the update performs.
    struct Requests {
        std::weak_ptr<Presentation> tapped;
        std::weak_ptr<Presentation> released;
        std::weak_ptr<Presentation> cycled;
    };

    PresentationView(Backend& drawing, Context& drawingContext);

    // Measures the screen of a presentation at the width of its area, finds where the anchor of a popover drew and keeps the frame for the gestures of the presentation.
    [[nodiscard]] Presentation::Frame prepare(Presentation& presentation, Gui& gui);

    // Returns how far the look of the presentation moves its screen and surface: a rise by the room under the top of its surface, a slide by the width of its surface toward the start edge.
    [[nodiscard]] math::Vec2 getOffset(const Presentation::Layout& layout, const Navigation::Look& look) const;

    // Draws the backdrop, the surface, the arrow and the grabber before the screen draws over them, and follows the drags and taps of the topmost presentation, or of a presentation that is not modal.
    void drawChrome(Presentation& presentation, const Presentation::Layout& layout, const Navigation::Look& look, math::Vec2 offset, bool interactive);

    // Returns how much the GUIs behind the presentation shrink, from the layout it last drew with and how far it arrived.
    [[nodiscard]] float getBehindScale(const Presentation& presentation, const Navigation::Look& look) const;

    [[nodiscard]] Requests takeRequests() noexcept;

    // Whether a request of the person waits for the next update.
    [[nodiscard]] bool hasRequests() const noexcept {
        return !requests.tapped.expired() || !requests.released.expired() || !requests.cycled.expired();
    }

  private:
    // How long a motion of the pointer counts toward the speed of a drag, in seconds.
    static constexpr float kSpeedWindow = 0.08F;

    void drawBackdrop(float shown);
    void drawSurface(const Presentation& presentation, const Presentation::Layout& layout, const Navigation::Look& look, math::Vec2 offset);
    void drawArrow(const Presentation::Layout& layout, math::Color color, float alpha);
    void followDrag(Presentation& presentation, ImGuiID item);
    void followBackdrop(Presentation& presentation, const math::Rect& surface, const Navigation::Look& look);

    // The distance a drag moves the surface toward where it closes, from the motion of the pointer since the press.
    [[nodiscard]] float getDragDistance(const Presentation& presentation, const ImVec2& dragged) const;

    Backend& backend;
    Context& context;
    Requests requests;
};

} // namespace haylen::ui
