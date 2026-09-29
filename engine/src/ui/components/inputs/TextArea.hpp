#pragma once

#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "ui/components/inputs/TextEntry.hpp"

namespace haylen::ui {

class TextArea final : public TextEntry {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "textArea";
    }

  protected:
    void readMore(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    int rows = 4;
};

} // namespace haylen::ui
