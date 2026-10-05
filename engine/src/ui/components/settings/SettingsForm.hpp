#pragma once

#include <cstddef>
#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// A column of settings rows and section titles with room between sections.
class SettingsForm final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "settingsForm";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return kUnlimitedChildren;
    }

  protected:
    void readProperties(PropertyReader&) override {}
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    // Section titles get extra space above them, except the first one.
    [[nodiscard]] static float getSpacingBefore(Context& context, const Component& child, bool first);
};

} // namespace haylen::ui
