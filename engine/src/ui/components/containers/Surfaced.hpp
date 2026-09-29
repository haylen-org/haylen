#pragma once

#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/ui/Theme.hpp"
#include "ui/components/containers/Linear.hpp"

namespace haylen::ui {

// A column on a themed surface that keeps the pointer from reaching the app behind it.
class Surfaced : public Linear {
  public:
    Surfaced(Theme::Surface role, Theme::Color fillColor, bool hasBorder) : Linear(false), surface(role), fill(fillColor), bordered(hasBorder) {}

  protected:
    [[nodiscard]] math::Insets getPadding(Context& context) const override;
    void paint(Context& context, const math::Rect& bounds) override;

  private:
    Theme::Surface surface;
    Theme::Color fill;
    bool bordered;
};

} // namespace haylen::ui
