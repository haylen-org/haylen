#pragma once

#include <cstddef>
#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// A row of buttons at the end of a form, such as cancel and save, which stack across the width of the form when they do not fit in one row.
class SettingsActions final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "settingsActions";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return kUnlimitedChildren;
    }

  protected:
    void readProperties(PropertyReader&) override {}
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    [[nodiscard]] bool isStacked(Context& context, float width);
};

} // namespace haylen::ui
