#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/Scrollbar.hpp"
#include "ui/ScrollFinger.hpp"
#include "ui/Scroller.hpp"

namespace haylen::ui {

// Shows one child in an area that scrolls up and down or sideways. The wheel, a finger dragging the content and the scroll bar that the app asks for scroll it, with the motion of a `Scroller`, the focus scrolls to the focused control, the command `scrollTo` scrolls to an offset or to a node inside it, and with snapping it settles on the start of the nearest item of its child. While the child is longer than the area, a bar that the app asks for takes a lane of its own beside it, and a scroll that fills stretches a shorter child to the length of the area.
class Scroll final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "scroll";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return 1;
    }
    void command(Context& context, std::string_view name, const core::Json& arguments) override;

  protected:
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    enum class Align : std::uint8_t {
        Nearest,
        Start,
        Center,
        End,
    };

    // A scroll the app asked for, to an offset from the start of the content or to a node inside the scroll, which follows the node while it moves and ends when the player scrolls.
    struct Request {
        std::optional<float> offset;
        std::string node;
        Align align = Align::Nearest;
        bool animated = true;
    };

    static constexpr std::array<std::pair<std::string_view, Align>, 4> kAligns{{
        {"nearest", Align::Nearest},
        {"start", Align::Start},
        {"center", Align::Center},
        {"end", Align::End},
    }};
    static constexpr float kSnapDelay = 0.12F;
    static constexpr double kSettleDistance = 0.5;

    [[nodiscard]] math::Rect place(Context& context, Component& child, math::Vec2 size, const math::Rect& view) const;

    // The scroll of the ImGui window follows the offset, and the offset follows a scroll that something else gave the window, such as the focus bringing a control into view, at once, so the next move of the focus starts from the control in view.
    void followWindow();
    void settle();

    [[nodiscard]] static const Component* findNode(const Component& parent, std::string_view id);

    // Returns the offset a request scrolls to, or nothing while its node does not show.
    [[nodiscard]] std::optional<double> findTarget(const Request& value, math::Vec2 origin, double view) const;

    bool scrollbarShown = false;
    bool horizontal = false;
    bool snap = false;
    bool fill = false;
    float idle = 0.0F;
    std::optional<float> written;
    Scroller scroller;
    ScrollFinger finger;
    std::vector<float> points;
    std::optional<Request> request;
    Scrollbar scrollbar;
};

} // namespace haylen::ui
