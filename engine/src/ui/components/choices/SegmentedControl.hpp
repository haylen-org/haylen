#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

// A row of joined segments of which one is selected, such as a filter between views. A click or tap picks a segment, left and right move the selection while it has the focus, and accept moves it to the next segment.
class SegmentedControl final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "segmentedControl";
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
    void collectPlayerValues(core::Json& values) const override {
        values["selected"] = selected;
    }

  private:
    [[nodiscard]] ImDrawFlags getCorners(const Context& context, std::size_t index) const noexcept;
    // Returns the index of the next item that can be picked in a direction, or the current one when there is none.
    [[nodiscard]] int findNext(int from, int direction, bool wrap) const;
    void select(Context& context, int index);

    std::vector<ChoiceItem> items;
    std::string selected;
};

} // namespace haylen::ui
