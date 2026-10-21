#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "haylen/math/Color.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Style.hpp"
#include "haylen/ui/Component.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

// A row of joined segments of which one is selected, such as a filter between views. A click or tap picks a segment, left and right move the selection while it has the focus, and accept moves it to the next segment.
class SegmentedControl final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "segmentedControl";
    }
    [[nodiscard]] bool describe(Context& context, platform::AccessibilityElement& element) const override;
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
    // Labels too wide for their segments draw smaller, down to this share of the size of their font, and wrap onto more lines below it.
    static constexpr float kMinLabelScale = 0.75F;

    [[nodiscard]] ImDrawFlags getCorners(const Context& context, std::size_t index) const noexcept;
    [[nodiscard]] math::Insets getPadding(Context& context) const;
    [[nodiscard]] float layOut(Context& context, float width);
    [[nodiscard]] static float getLongestWord(Context& context, std::string_view label);
    [[nodiscard]] text::Style getLabelStyle(Context& context, float width) const;
    void drawLabel(Context& context, const math::Rect& area, std::string_view label, math::Color color) const;
    // Returns the index of the next item that can be picked in a direction, or the current one when there is none.
    [[nodiscard]] int findNext(int from, int direction, bool wrap) const;
    void select(Context& context, int index);

    std::vector<ChoiceItem> items;
    std::string selected;

    // The width of every segment and of the longest word of its label in the last layout, kept so a frame reuses the storage, the share of the size of their font the labels draw at and whether they wrap.
    std::vector<float> widths;
    std::vector<float> words;
    float labelScale = 1.0F;
    bool labelsWrap = false;
};

} // namespace haylen::ui
