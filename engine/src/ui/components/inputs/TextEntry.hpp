#pragma once

#include <optional>
#include <string>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/TextInput.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"

namespace haylen::ui {

// The value, placeholder, length limit and keyboard options every text entry shares. The value the player types stays in the component, so a later patch of other properties keeps it.
class TextEntry : public Component {
  protected:
    [[nodiscard]] bool isFocusable() const noexcept override {
        return true;
    }

    void readProperties(PropertyReader& reader) override;
    virtual void readMore(PropertyReader&) {}
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;

    // Draws the editor with the keyboard of the entry and space kept free for icons on either side.
    void drawEntry(Context& context, const math::Rect& bounds, platform::TextInput::Keyboard keyboard, float reserveStart = 0.0F, float reserveEnd = 0.0F);

    std::string value;

  private:
    // Prose keyboards correct and capitalize by default, while addresses, numbers and passwords keep the text exactly as typed.
    [[nodiscard]] platform::TextInput::Options getInputOptions(platform::TextInput::Keyboard keyboard) const noexcept;

    TextValue placeholder;
    int maxLength = 0;
    platform::TextInput::ReturnKey returnKey = platform::TextInput::ReturnKey::Default;
    std::optional<platform::TextInput::Capitalization> capitalization;
    std::optional<bool> autocorrect;
};

} // namespace haylen::ui
