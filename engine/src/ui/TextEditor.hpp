#pragma once

#include <array>
#include <string>
#include <string_view>
#include <utility>

#include <imgui.h>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/TextInput.hpp"
#include "haylen/text/Style.hpp"
#include "ui/TextFieldLayout.hpp"

namespace haylen::ui {

class Context;
class Scrollbar;
class TextSession;

// Draws a themed text editor of one or many lines over Dear ImGui text input, with its own surface and focus ring. ImGui keeps the text, the undo history and the keys that edit it, while the editor draws the text shaped and ordered for display, its selection and its caret, and places the caret by the shaped text when the pointer or the arrow keys move it. The focused editor edits through the text session of the backend, so the native field of the platform types into it.
class TextEditor final {
  public:
    struct Options {
        std::string_view placeholder;
        platform::TextInput::Options input;
        bool focus = false;
        float reserveStart = 0.0F;
        float reserveEnd = 0.0F;

        // The scroll bar of an editor of many lines, which shows while its text is taller than its area.
        Scrollbar* scrollbar = nullptr;
    };

    struct Result {
        bool changed = false;
        bool submitted = false;
    };

    static Result draw(Context& context, const math::Rect& bounds, std::string& value, const Options& options);

    // Lays out the text a field shows, where a password shows one bullet per character.
    [[nodiscard]] static TextFieldLayout layOut(Context& context, std::string_view value, const text::Style& style, bool password);
    [[nodiscard]] static std::string getShown(std::string_view value, bool password);

  private:
    // The caret and the anchor of the selection are where they stood before this frame, in bytes, or -1 when the field was not focused.
    struct Editing {
        TextSession* session = nullptr;
        Context* context = nullptr;
        text::Style style;
        math::Rect bounds;
        math::Rect area;
        math::Vec2 scroll{};
        bool rightToLeft = false;
        bool password = false;
        bool multiline = false;
        int cursor = -1;
        int anchor = -1;
    };

    // The caret keeps this far from the edges of the field while the text scrolls under it.
    static constexpr float kCaretMargin = 2.0F;
    static constexpr std::string_view kBullet = "\xE2\x80\xA2";

    static int callback(ImGuiInputTextCallbackData* data);
    static void placeCaret(ImGuiInputTextCallbackData& data, const Editing& editing);

    // Returns the top-left of the text block in the area, aligned where its direction starts and moved by the scroll.
    [[nodiscard]] static math::Vec2 getOrigin(const TextFieldLayout& field, const math::Rect& area, math::Vec2 scroll, bool rightToLeft, bool multiline);

    // Moves the scroll so the caret stays inside the area, without leaving room at an end the text does not fill.
    static void follow(math::Vec2& scroll, const TextFieldLayout& field, const math::Rect& caret, const math::Rect& area, bool rightToLeft, bool multiline);

    // Underlines the text the input method composes in the focused field.
    static void drawComposition(const TextSession& session, ImGuiID id, const TextFieldLayout& field, math::Vec2 origin);
    static void limit(std::string& value, int maxLength);
};

} // namespace haylen::ui
