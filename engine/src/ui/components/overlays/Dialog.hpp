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

// The card of a dialog with a title, a message, optional content and answer buttons, which line up in one row while they fit and otherwise stack across its width, on its themed surface or, with the property `glass`, on glass. A presentation of the dialog style shows it over the dimmed screen, which takes the modality, the backdrop, the fades and the back. The card is as wide as the theme `dialogWidth` within the room it has, as tall as its content within the safe area, and content taller than that scrolls beside a scroll bar at the edge of the card. Its last button takes the focus when the card first draws.
class Dialog final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "dialog";
    }
    [[nodiscard]] bool describe(Context& context, platform::AccessibilityElement& element) const override;
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return kUnlimitedChildren;
    }

  protected:
    [[nodiscard]] Alignment getDefaultAlignment() const noexcept override {
        return Alignment::Center;
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
    [[nodiscard]] float getBodyHeight(Context& context, float width);
    [[nodiscard]] float getContentHeight(Context& context, float width);
    [[nodiscard]] bool isStacked(Context& context, float width) const;
    [[nodiscard]] static float getStackedButtonHeight(Context& context, const Answer& button, float width);
    [[nodiscard]] float getButtonsHeight(Context& context, float width) const;
    void drawContent(Context& context, const math::Rect& frame, const math::Insets& padding, bool overflowing);
    void drawBody(Context& context, const math::Rect& body, const math::Rect& inner, bool overflowing);

    bool glass = false;
    bool focused = false;
    TextValue title;
    TextValue message;
    std::vector<Answer> buttons;
    Scrollbar scrollbar;
};

} // namespace haylen::ui
