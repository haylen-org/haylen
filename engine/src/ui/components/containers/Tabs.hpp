#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

// A strip of tabs over the child of the selected tab. Children follow the order of the items.
class Tabs final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "tabs";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return kUnlimitedChildren;
    }

  protected:
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    [[nodiscard]] float getTabWidth(Context& context, const ChoiceItem& item) const;
    [[nodiscard]] int getSelectedIndex() const;
    [[nodiscard]] Component* getSelectedChild() const;

    std::vector<ChoiceItem> items;
    std::string selected;
};

} // namespace haylen::ui
