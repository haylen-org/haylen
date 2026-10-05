#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

// A menu of items that opens over its one child: with a right click, a long press on a touch screen, `uiMenu` while the focus is inside the child, or the `open` command.
class ContextMenu final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "contextMenu";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return 1;
    }

    void command(Context& context, std::string_view name, const core::Json& arguments) override;

  protected:
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;
    void drawingStopped(Context&) override;

  private:
    [[nodiscard]] std::optional<math::Vec2> findOpening(Context& context, const math::Rect& bounds);

    std::vector<ChoiceItem> items;
    bool openRequested = false;
    bool pressHandled = false;
};

} // namespace haylen::ui
