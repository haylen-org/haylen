#pragma once

#include <cstddef>
#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"

namespace haylen::ui {

// A setting with its label and caption on the left and its control on the right.
class SettingsRow final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "settingsRow";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return 1;
    }

  protected:
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    TextValue label;
    TextValue caption;
};

} // namespace haylen::ui
