#pragma once

#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

class StatusIndicator final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "statusIndicator";
    }

  protected:
    [[nodiscard]] Alignment getDefaultAlignment() const noexcept override {
        return Alignment::Start;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    [[nodiscard]] static float getDotSize(Context& context);

    TextValue text;
    Widgets::Tone tone = Widgets::Tone::Success;
};

} // namespace haylen::ui
