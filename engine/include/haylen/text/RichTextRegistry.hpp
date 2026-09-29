#pragma once

#include <array>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Effect.hpp"
#include "haylen/text/FontFamily.hpp"

namespace haylen::text {

// The names rich text markup refers to across an app: the effects that tags such as [wave] run, with the built-in effects registered from the start, and the icons of [icon=name], such as input prompts. It also holds the family of the default font.
class RichTextRegistry final {
  public:
    // An image region shown inline by [icon=name]. An icon without a size is as tall as the text and keeps the shape of its region.
    struct Icon {
        graphics::Texture texture;
        math::Rect source{};
        math::Vec2 size{};
    };

    explicit RichTextRegistry(std::shared_ptr<FontFamily> defaultFamily);

    [[nodiscard]] const std::shared_ptr<FontFamily>& getDefaultFamily() const noexcept {
        return family;
    }

    // Registers an effect under a tag name, replacing an effect the app registered before. Markup tags and built-in effects keep their names.
    void registerEffect(std::string name, Effect::Function effect);
    [[nodiscard]] const Effect::Function* findEffect(std::string_view name) const noexcept;
    [[nodiscard]] std::vector<std::string> getEffectNames() const;

    void registerIcon(std::string name, Icon icon);
    [[nodiscard]] const Icon* findIcon(std::string_view name) const noexcept;

    // Forgets the effects and icons the app registered and keeps the built-in effects, which the engine does before the Lua state closes.
    void clear();

  private:
    static constexpr std::array<std::string_view, 6> kBuiltInEffects{"wave", "shake", "tornado", "fade", "rainbow", "pulse"};

    void registerBuiltInEffects();

    std::shared_ptr<FontFamily> family;
    std::map<std::string, Effect::Function, std::less<>> effects;
    std::map<std::string, Icon, std::less<>> icons;
};

} // namespace haylen::text
