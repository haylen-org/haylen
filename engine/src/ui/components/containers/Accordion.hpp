#pragma once

#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

// Sections under headers that open and close, one child per item in the order of the items. Opening a section closes the others unless several may stay open.
class Accordion final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "accordion";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return kUnlimitedChildren;
    }

  protected:
    [[nodiscard]] bool isFocusable() const noexcept override {
        return true;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    [[nodiscard]] Component* getSection(std::size_t index) const;
    void toggle(Context& context, const ChoiceItem& item);

    std::vector<ChoiceItem> items;
    std::set<std::string, std::less<>> expanded;
    bool multiple = false;
};

} // namespace haylen::ui
