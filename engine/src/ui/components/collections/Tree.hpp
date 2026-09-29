#pragma once

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

// Nested items that open and close. The open items start from the expanded property and then follow the player. Every row takes the focus, and right opens and left closes the focused item.
class Tree final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "tree";
    }
    [[nodiscard]] bool usesFocusDirection(FocusDirection direction) const noexcept override {
        return direction == FocusDirection::Left || direction == FocusDirection::Right;
    }

  protected:
    [[nodiscard]] bool isFocusable() const noexcept override {
        return true;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    [[nodiscard]] std::size_t countVisible(const std::vector<ChoiceItem>& branch) const;
    void drawItems(Context& context, const math::Rect& bounds, const std::vector<ChoiceItem>& branch, int depth, float& y);
    void toggle(Context& context, const ChoiceItem& item);

    std::vector<ChoiceItem> items;
    std::string selected;
    std::set<std::string, std::less<>> expanded;
    std::optional<FocusDirection> pressedDirection;
    bool focusing = false;
};

} // namespace haylen::ui
