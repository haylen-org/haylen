#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

class Grid final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "grid";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return kUnlimitedChildren;
    }

  protected:
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    struct Layout {
        float cell = 0.0F;
        float widest = 0.0F;
        std::vector<float> rows;
    };

    [[nodiscard]] float getGap(Context& context) const;
    [[nodiscard]] Layout measureRows(Context& context, float width);

    int columns = 2;
    std::optional<float> gap;
    math::Insets padding;
};

} // namespace haylen::ui
