#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "haylen/core/Json.hpp"
#include "haylen/platform/TextInput.hpp"

namespace haylen::platform {

// Text input of the web runtime. A hidden textarea, or a password input, lies over the focused field next to the canvas and edits it, which brings the software keyboard of phones, the input methods of every browser and native paste, copy and undo. platform/web/haylen-runtime.js holds the page side.
class WebTextInput final : public TextInput {
  public:
    [[nodiscard]] bool isNative() const noexcept override {
        return true;
    }
    void edit(const Field& field) override;
    void finish() override;
    void setVisibleFields(std::span<const Field> fields) override;

    // Requests of the page, which reach the running app on its next frame.
    static void receiveEdit(double field, double revision, const char* text, int selectionStart, int selectionEnd, int compositionStart, int compositionEnd);
    static void receiveAction(double field, int action);
    static void receiveKeyboard(float x, float y, float width, float height);

  private:
    // Describes a field with the attributes of its element: inputmode, enterkeyhint, autocapitalize, autocorrect and maxlength.
    [[nodiscard]] static core::Json toJson(const Field& field);
    [[nodiscard]] static std::string_view getInputMode(Keyboard keyboard) noexcept;
    [[nodiscard]] static std::string_view getEnterKeyHint(ReturnKey key) noexcept;
    [[nodiscard]] static std::string_view getCapitalization(Capitalization capitalization) noexcept;
};

} // namespace haylen::platform
