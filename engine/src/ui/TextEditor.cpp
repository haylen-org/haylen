#include "ui/TextEditor.hpp"

#include <algorithm>
#include <cfloat>
#include <optional>

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "haylen/core/Utf8.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
#include "ui/TextSession.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

TextEditor::Result TextEditor::draw(Context& context, const math::Rect& bounds, std::string& value, const Options& options) {
    const ImGuiID id = ImGui::GetID("##field");
    const bool focused = ImGui::GetActiveID() == id;
    const bool password = options.input.keyboard == platform::TextInput::Keyboard::Password;
    const bool multiline = options.input.keyboard == platform::TextInput::Keyboard::Multiline;
    TextSession& session = context.getBackend().getTextSession();
    session.addField(id, bounds, options.input);
    Surfaces::draw(context, focused ? Theme::Surface::FieldFocused : Theme::Surface::Field, bounds, context.getColor(Theme::Color::Raised), context.getColor(focused ? Theme::Color::Focus : Theme::Color::Border));

    const math::Rect padded = bounds.inset(Surfaces::getPadding(context, focused ? Theme::Surface::FieldFocused : Theme::Surface::Field));
    const math::Rect inner = math::Rect::fromMinMax({padded.x + options.reserveLeft, padded.y}, {std::max(padded.x + options.reserveLeft, padded.getRight() - options.reserveRight), padded.getBottom()});
    const float paddingX = context.getMetric(Theme::Metric::ControlPaddingX) * 0.5F;
    const float fontSize = context.getFontSize(Theme::Font::Body);
    const float paddingY = multiline ? context.getMetric(Theme::Metric::ControlPaddingY) : std::max(0.0F, (inner.height - fontSize) * 0.5F);

    ImGui::SetCursorScreenPos(ImGuiConverter::toImVec2(inner.getMin()));
    ImGui::PushFont(context.getFont(Theme::Font::Body), fontSize);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGuiConverter::toImVec4(context.getColor(Theme::Color::Text)));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImGuiConverter::toImVec4(context.getColor(Theme::Color::TextMuted)));
    // The field draws its own focus ring around the whole field instead of the one ImGui draws around the editor.
    ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {paddingX, paddingY});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0F);
    if (options.focus) {
        ImGui::SetKeyboardFocusHere();
    }

    const std::string before = value;
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackEdit;
    if (password) {
        flags |= ImGuiInputTextFlags_Password;
    }
    bool submitted = false;
    if (multiline) {
        // ImGui shows hints on single lines only, so an empty text area draws its placeholder behind the transparent editor.
        if (value.empty() && !options.placeholder.empty()) {
            Typography::draw(context, Theme::Font::Body, {inner.x + paddingX, inner.y + paddingY}, context.getColor(Theme::Color::TextMuted), options.placeholder);
        }
        (void)ImGui::InputTextMultiline("##field", &value, ImGuiConverter::toImVec2(inner.getSize()), flags, &TextSession::callback, &session);
    } else {
        ImGui::SetNextItemWidth(inner.width);
        submitted = ImGui::InputTextWithHint("##field", std::string(options.placeholder).c_str(), &value, flags | ImGuiInputTextFlags_EnterReturnsTrue, &TextSession::callback, &session);
    }

    // A submit, cancel or dismiss of the platform ends the editing, and only a submit reports the text.
    if (const std::optional<platform::TextInput::Action> action = session.takeAction(id)) {
        submitted = submitted || action == platform::TextInput::Action::Submit;
        ImGui::ClearActiveID();
    }

    // ImGui places the caret of the focused editor while it draws it, which is where the platform shows its input method.
    const ImGuiPlatformImeData& ime = ImGui::GetCurrentContext()->PlatformImeData;
    if (ImGui::GetActiveID() == id && ime.WantVisible) {
        const math::Rect caret{ime.InputPos.x + 1.0F, ime.InputPos.y, 1.0F, ime.InputLineHeight};
        session.setCaret(caret);
        if (!password) {
            drawComposition(session, id, value, caret, inner);
        }
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::PopFont();
    context.getFocus().addTarget(id, bounds);
    Widgets::drawFocusRing(context, bounds, id, context.getMetric(Theme::Metric::ControlRadius));

    limit(value, options.input.maxLength);
    return {.changed = value != before, .submitted = submitted};
}

void TextEditor::drawComposition(const TextSession& session, ImGuiID id, std::string_view text, const math::Rect& caret, const math::Rect& clip) {
    const std::optional<std::pair<int, int>> composition = session.getComposition(id);
    if (!composition) {
        return;
    }

    // The composition sits on the line of the caret, so each end is measured from the caret and stops at a line break.
    const std::size_t caretOffset = core::Utf8::getOffset(text, static_cast<std::size_t>(session.getCaret()));
    const std::size_t newline = text.substr(0, caretOffset).rfind('\n');
    const std::size_t lineStart = newline == std::string_view::npos ? 0 : newline + 1;
    const std::size_t lineEnd = std::min(text.find('\n', caretOffset), text.size());
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    // clang-format off
    const auto place = [&](int index) {
        const std::size_t offset = std::clamp(core::Utf8::getOffset(text, static_cast<std::size_t>(index)), lineStart, lineEnd);
        const float width = font->CalcTextSizeA(size, FLT_MAX, 0.0F, text.data() + std::min(offset, caretOffset), text.data() + std::max(offset, caretOffset)).x;
        return offset < caretOffset ? caret.x - width : caret.x + width;
    };
    // clang-format on

    const float baseline = caret.getBottom() - 1.0F;
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.PushClipRect(ImGuiConverter::toImVec2(clip.getMin()), ImGuiConverter::toImVec2(clip.getMax()), true);
    list.AddLine({place(composition->first), baseline}, {place(composition->second), baseline}, ImGui::GetColorU32(ImGuiCol_Text), std::max(1.0F, size / 14.0F));
    list.PopClipRect();
}

// A length limit counts code points and never cuts a character in half.
void TextEditor::limit(std::string& value, int maxLength) {
    if (maxLength > 0) {
        value.resize(core::Utf8::getOffset(value, static_cast<std::size_t>(maxLength)));
    }
}

} // namespace haylen::ui
