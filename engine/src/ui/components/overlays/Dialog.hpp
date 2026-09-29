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
#include "ui/Widgets.hpp"

namespace haylen::ui {

// A modal window over the whole screen with a title, a message, optional content and answer buttons. It takes no room in the layout that holds it.
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
    [[nodiscard]] float getContentHeight(Context& context, float width);
    void drawContent(Context& context, const math::Rect& inner);
    void drawBody(Context& context);

    bool open = false;
    bool dismissible = true;
    TextValue title;
    TextValue message;
    std::vector<Answer> buttons;
};

} // namespace haylen::ui
