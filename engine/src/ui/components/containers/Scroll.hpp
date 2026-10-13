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
#include "ui/Scrollbar.hpp"

namespace haylen::ui {

// Shows one child in an area that scrolls up and down or sideways. The wheel, the scroll bar and a finger dragging the content scroll it, the focus scrolls to the focused control, the command `scrollTo` scrolls to an offset or to a node inside it, and with snapping it settles on the start of the nearest item of its child. While the child is longer than the area, the bar takes a lane of its own beside it.
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
    static constexpr float kSnapSpeed = 14.0F;

    [[nodiscard]] math::Rect place(Context& context, Component& child, math::Vec2 size, const math::Rect& view) const;

    // A finger that drags the content takes over from the control it pressed, so a list of buttons still scrolls on a touch screen.
    void followFinger();
    void settle(Context& context, float position, float limit);

    [[nodiscard]] static const Component* findNode(const Component& parent, std::string_view id);

    // Returns the offset a request scrolls to, or nothing while its node does not show.
    [[nodiscard]] std::optional<float> findTarget(const Request& value, math::Vec2 origin, float view, float limit) const;

    // Moves the offset toward the target, the whole way at once or a share of it each frame, and returns whether it arrived.
    bool approach(Context& context, float target, bool animated);

    bool scrollbarShown = true;
    bool horizontal = false;
    bool snap = false;
    float idle = 0.0F;
    std::vector<float> points;
    std::optional<Request> request;
    Scrollbar scrollbar;
};

} // namespace haylen::ui
