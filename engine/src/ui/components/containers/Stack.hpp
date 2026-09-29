#pragma once

#include <cstddef>
#include <string_view>

#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
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
    math::Insets padding;
};

} // namespace haylen::ui
