#pragma once

#include <string>
#include <string_view>

#include "haylen/core/Json.hpp"

namespace haylen::ui {

// Text a component shows: a literal, or a localization key with arguments that is translated whenever it is drawn, so a language change shows at once.
struct TextValue {
    std::string literal;
    std::string key;
    core::Json arguments = core::Json::object();

    // Reads text, a number or a translation such as `{key = 'menu.play'}`, naming the property as `context` in errors.
    [[nodiscard]] static TextValue fromJson(const core::Json& value, std::string_view context);

    [[nodiscard]] bool isEmpty() const noexcept {
        return literal.empty() && key.empty();
    }

  private:
    [[nodiscard]] static std::string formatNumber(const core::Json& value);
};

} // namespace haylen::ui
