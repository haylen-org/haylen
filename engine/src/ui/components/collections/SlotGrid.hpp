#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"

namespace haylen::ui {

// A grid of square slots that hold pictures and counts, such as an inventory or a hotbar. The pointer drags items between slots and lists of any GUI, and the keyboard, gamepads and remotes carry them with accept. The grid only reports moves, so the app moves its own items.
class SlotGrid final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "slotGrid";
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
    void drawingStopped(Context& context) override;

  private:
    struct Slot {
        std::string id;
        std::string image;
        TextValue count;
        bool enabled = true;
    };

    [[nodiscard]] static std::vector<Slot> readSlots(PropertyReader& reader, const core::Json& value);
    [[nodiscard]] float getSlotSize(Context& context) const;
    [[nodiscard]] float getGap(Context& context) const;
    void drawSlot(Context& context, const Slot& slot, const math::Rect& area, bool focusTarget);

    // Paints the surface, the picture and the count of a slot in view.
    void drawContent(Context& context, const Slot& slot, const math::Rect& area, bool hovered) const;

    std::vector<Slot> slots;
    std::string selected;
    int columns = 4;
    float slotSize = 0.0F;
    std::optional<float> gap;
    bool draggable = true;
    std::string dragging;
};

} // namespace haylen::ui
