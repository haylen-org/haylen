#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"
#include "ui/Scrollbar.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

// A modal window over the whole screen with a title, a message, optional content and answer buttons, over a backdrop that fades in and out with it. It takes no room in the layout that holds it, and content taller than the screen scrolls beside a scroll bar at the edge of the dialog.
class Dialog final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "dialog";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return kUnlimitedChildren;
    }

  protected:
    [[nodiscard]] bool isFloating() const noexcept override {
        return true;
    }
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context&, float) override;
    void render(Context& context, const math::Rect&) override;

  private:
    struct Answer {
        std::string id;
        TextValue text;
        Widgets::ButtonVariant variant = Widgets::ButtonVariant::Default;
    };

    [[nodiscard]] static std::vector<Answer> readButtons(PropertyReader& reader, const core::Json& value);
    [[nodiscard]] static math::Insets getContentPadding(Context& context);
    [[nodiscard]] static bool isWaiting();
    static void drawBackdrop(Context& context, const math::Rect& display);
    static void close();
    [[nodiscard]] float getBodyHeight(Context& context, float width);
    [[nodiscard]] float getContentHeight(Context& context, float width);
    void drawContent(Context& context, const math::Rect& frame, const math::Insets& padding);
    void drawBody(Context& context, const math::Rect& body, const math::Rect& inner);

    bool open = false;
    bool dismissible = true;
    float shown = 0.0F;
    TextValue title;
    TextValue message;
    std::vector<Answer> buttons;
    Scrollbar scrollbar;
};

} // namespace haylen::ui
