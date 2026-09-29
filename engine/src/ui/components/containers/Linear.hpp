#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// Lays children out in a line, down a column or across a row, sharing extra space among children that grow and placing the rest by justify.
class Linear : public Component {
  public:
    explicit Linear(bool isHorizontal) : horizontal(isHorizontal) {}

    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return kUnlimitedChildren;
    }

  protected:
    void readProperties(PropertyReader& reader) override;
    virtual void readMore(PropertyReader&) {}
    virtual void paint(Context&, const math::Rect&) {}
    [[nodiscard]] virtual math::Insets getPadding(Context&) const;
    [[nodiscard]] float getGap(Context& context) const;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    enum class Justify : std::uint8_t {
        Start,
        Center,
        End,
        SpaceBetween,
    };

    static constexpr std::array<std::pair<std::string_view, Justify>, 4> kJustify{{
        {"start", Justify::Start},
        {"center", Justify::Center},
        {"end", Justify::End},
        {"spaceBetween", Justify::SpaceBetween},
    }};

    bool horizontal;
    std::optional<float> gap;
    math::Insets padding;
    Justify justify = Justify::Start;
};

} // namespace haylen::ui
