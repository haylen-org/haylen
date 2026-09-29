#pragma once

#include <string>
#include <string_view>

#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"

namespace haylen::ui {

// A picture that works as a button, with optional pictures for hover and press and optional text on top.
class ImageButton final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "imageButton";
    }

  protected:
    [[nodiscard]] Alignment getDefaultAlignment() const noexcept override {
        return Alignment::Start;
    }
    [[nodiscard]] bool isFocusable() const noexcept override {
        return true;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    std::string image;
    std::string hoverImage;
    std::string pressedImage;
    TextValue text;
    float scale = 1.0F;
    math::Color tint = math::Color::white();
};

} // namespace haylen::ui
