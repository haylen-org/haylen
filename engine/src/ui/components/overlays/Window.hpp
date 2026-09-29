#pragma once

#include <optional>
#include <string_view>

#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/TextValue.hpp"
#include "ui/components/containers/Linear.hpp"

namespace haylen::ui {

// A floating window with a title bar the pointer drags around, holding a column of children, such as an inventory or a map over the game. It keeps the focus inside while it has it, and cancel closes it when it is closable.
class Window final : public Linear {
  public:
    Window() : Linear(false) {}

    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "window";
    }

  protected:
    [[nodiscard]] bool isFloating() const noexcept override {
        return true;
    }

    void readMore(PropertyReader& reader) override;
    [[nodiscard]] math::Insets getPadding(Context& context) const override;
    [[nodiscard]] math::Vec2 measureContent(Context&, float) override;
    void render(Context& context, const math::Rect&) override;

  private:
    [[nodiscard]] math::Vec2 getSize(Context& context);
    void drawTitle(Context& context, const math::Rect& bar);
    void close(Context& context);

    TextValue title;
    bool open = true;
    bool closable = false;
    bool movable = true;
    std::optional<math::Vec2> requested;
    std::optional<math::Vec2> position;
    bool dragging = false;
};

} // namespace haylen::ui
