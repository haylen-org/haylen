#pragma once

#include <string>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

// The text, icon and variant every button-like kind shares.
class ButtonBase : public Component {
  protected:
    [[nodiscard]] Alignment getDefaultAlignment() const noexcept override {
        return Alignment::Start;
    }
    [[nodiscard]] bool isFocusable() const noexcept override {
        return true;
    }

    void readProperties(PropertyReader& reader) override;
    virtual void readMore(PropertyReader&) {}
    [[nodiscard]] math::Vec2 measureContent(Context& context, float) override;
    [[nodiscard]] bool drawButton(Context& context, const math::Rect& bounds, bool checked = false);

  private:
    TextValue text;
    std::string icon;
    Widgets::ButtonVariant variant = Widgets::ButtonVariant::Default;
};

} // namespace haylen::ui
