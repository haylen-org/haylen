#pragma once

#include <array>
#include <string>
#include <string_view>
#include <utility>

#include <imgui.h>

#include "haylen/math/Rect.hpp"
#include "haylen/platform/TextInput.hpp"

namespace haylen::ui {

class Context;
class TextSession;

// Draws a themed text editor of one or many lines over Dear ImGui text input, with its own surface and focus ring. The focused editor edits through the text session of the backend, so the native field of the platform types into it.
class TextEditor final {
  public:
    struct Options {
        std::string_view placeholder;
        platform::TextInput::Options input;
        bool focus = false;
        float reserveLeft = 0.0F;
        float reserveRight = 0.0F;
    };

    struct Result {
        bool changed = false;
        bool submitted = false;
    };

    // The names that the returnKey and autocapitalize properties of text components take.
    static constexpr std::array<std::pair<std::string_view, platform::TextInput::ReturnKey>, 6> kReturnKeys{{
        {"default", platform::TextInput::ReturnKey::Default},
        {"done", platform::TextInput::ReturnKey::Done},
        {"go", platform::TextInput::ReturnKey::Go},
        {"next", platform::TextInput::ReturnKey::Next},
        {"search", platform::TextInput::ReturnKey::Search},
        {"send", platform::TextInput::ReturnKey::Send},
    }};
    static constexpr std::array<std::pair<std::string_view, platform::TextInput::Capitalization>, 4> kCapitalizations{{
        {"none", platform::TextInput::Capitalization::None},
        {"sentences", platform::TextInput::Capitalization::Sentences},
        {"words", platform::TextInput::Capitalization::Words},
        {"characters", platform::TextInput::Capitalization::Characters},
    }};

    static Result draw(Context& context, const math::Rect& bounds, std::string& value, const Options& options);

  private:
    // Underlines the text the input method composes in the focused field.
    static void drawComposition(const TextSession& session, ImGuiID id, std::string_view text, const math::Rect& caret, const math::Rect& clip);
    static void limit(std::string& value, int maxLength);
};

} // namespace haylen::ui
