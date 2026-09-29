#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

// A short notice at the top or bottom of the safe area that fades out after its duration. Setting open again shows it again from the start.
class Toast final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "toast";
    }

  protected:
    [[nodiscard]] bool isFloating() const noexcept override {
        return true;
    }
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context&, float) override;
    void render(Context& context, const math::Rect&) override;

  private:
    enum class Position : std::uint8_t {
        Top,
        Bottom,
    };

    static constexpr std::array<std::pair<std::string_view, Position>, 2> kPositions{{
        {"top", Position::Top},
        {"bottom", Position::Bottom},
    }};
    static constexpr float kBar = 6.0F;

    bool open = false;
    TextValue text;
    Widgets::Tone tone = Widgets::Tone::Information;
    float duration = 3.0F;
    Position position = Position::Top;
    double shownAt = -1.0;
};

} // namespace haylen::ui
