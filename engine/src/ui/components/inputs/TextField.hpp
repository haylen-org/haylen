#pragma once

#include <array>
#include <string_view>
#include <utility>

#include "haylen/math/Rect.hpp"
#include "haylen/platform/TextInput.hpp"
#include "ui/components/inputs/TextEntry.hpp"

namespace haylen::ui {

// A single line of text, typed with the on-screen keyboard its keyboard property names.
class TextField final : public TextEntry {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "textField";
    }

  protected:
    void readMore(PropertyReader& reader) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    // Multiline and password keyboards belong to `textArea` and `secretField`.
    static constexpr std::array<std::pair<std::string_view, platform::TextInput::Keyboard>, 7> kKeyboards{{
        {"text", platform::TextInput::Keyboard::Text},
        {"number", platform::TextInput::Keyboard::Number},
        {"decimal", platform::TextInput::Keyboard::Decimal},
        {"phone", platform::TextInput::Keyboard::Phone},
        {"email", platform::TextInput::Keyboard::Email},
        {"url", platform::TextInput::Keyboard::Url},
        {"search", platform::TextInput::Keyboard::Search},
    }};

    platform::TextInput::Keyboard keyboard = platform::TextInput::Keyboard::Text;
};

} // namespace haylen::ui
