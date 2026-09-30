#pragma once

#include <string_view>

#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"

namespace haylen::ui {

// A page title with an optional caption, drawn on the `banner` surface when `banner` is set, such as a ribbon in a textured theme.
class PageHeader final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "pageHeader";
    }

  protected:
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    [[nodiscard]] math::Insets getInsets(Context& context) const;

    TextValue title;
    TextValue caption;
    bool banner = false;
    Alignment textAlign = Alignment::Start;
};

} // namespace haylen::ui
