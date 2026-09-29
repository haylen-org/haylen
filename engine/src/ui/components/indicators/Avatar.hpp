#pragma once

#include <string>
#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"

namespace haylen::ui {

// A round picture of a player, or the initials of a name when there is no picture.
class Avatar final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "avatar";
    }

  protected:
    [[nodiscard]] Alignment getDefaultAlignment() const noexcept override {
        return Alignment::Start;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context&, float) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    // Takes the first letter of the first two words.
    [[nodiscard]] static std::string getInitials(std::string_view fullName);

    std::string image;
    TextValue name;
    float size = 64.0F;
};

} // namespace haylen::ui
