#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

class Image final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "image";
    }

  protected:
    [[nodiscard]] Alignment getDefaultAlignment() const noexcept override {
        return Alignment::Start;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    enum class Fit : std::uint8_t {
        Contain,
        Cover,
        Fill,
    };

    static constexpr std::array<std::pair<std::string_view, Fit>, 3> kFits{{
        {"contain", Fit::Contain},
        {"cover", Fit::Cover},
        {"fill", Fit::Fill},
    }};

    std::string image;
    Fit fit = Fit::Contain;
    float scale = 1.0F;
    math::Color tint = math::Color::white();
    float radius = 0.0F;
};

} // namespace haylen::ui
