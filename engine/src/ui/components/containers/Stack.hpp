#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// Draws its children on top of each other, the later ones above, each placed by its own alignment.
class Stack final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "stack";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return kUnlimitedChildren;
    }

  protected:
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    static constexpr std::array<std::pair<std::string_view, Alignment>, 4> kAlignments{{
        {"start", Alignment::Start},
        {"center", Alignment::Center},
        {"end", Alignment::End},
        {"stretch", Alignment::Stretch},
    }};

    math::Insets padding;
    std::optional<Alignment> alignItems;
};

} // namespace haylen::ui
