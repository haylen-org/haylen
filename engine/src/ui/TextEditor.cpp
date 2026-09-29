#include "ui/TextEditor.hpp"

#include <algorithm>
#include <cmath>
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

std::string TextEditor::getShown(std::string_view value, bool password) {
    if (!password) {
        return std::string(value);
    }
    std::string bullets;
    for (std::size_t index = core::Utf8::countCodePoints(value); index > 0; --index) {
        bullets += kBullet;
    }
    return bullets;
}

TextFieldLayout TextEditor::layOut(Context& context, std::string_view value, const text::TextStyle& style, bool password) {
    const std::string shown = getShown(value, password);
    return {Typography::layout(context, Theme::Font::Body, shown, style), core::Utf8::decode(shown)};
}

math::Vec2 TextEditor::getOrigin(const TextFieldLayout& field, const math::Rect& area, math::Vec2 scroll, bool rightToLeft, bool multiline) {
    const math::Vec2 size = field.getLayout().size;
    const float x = rightToLeft ? area.getRight() - size.x : area.x;
    const float y = multiline ? area.y : area.y + (area.height - size.y) * 0.5F;
    return {x + scroll.x, y - scroll.y};
}

// A left-to-right text scrolls to the left and a right-to-left one to the right, and neither scrolls further than its overflow.
void TextEditor::follow(math::Vec2& scroll, const TextFieldLayout& field, const math::Rect& caret, const math::Rect& area, bool rightToLeft, bool multiline) {
    if (caret.x > area.getRight() - kCaretMargin) {
        scroll.x -= caret.x - (area.getRight() - kCaretMargin);
    } else if (caret.x < area.x + kCaretMargin) {
        scroll.x += area.x + kCaretMargin - caret.x;
    }
    const math::Vec2 size = field.getLayout().size;
    const float overflow = std::max(0.0F, size.x + kCaretMargin * 2.0F - area.width);
    scroll.x = rightToLeft ? std::clamp(scroll.x, 0.0F, overflow) : std::clamp(scroll.x, -overflow, 0.0F);
    if (!multiline) {
        return;
    }
    if (caret.y < area.y) {
        scroll.y -= area.y - caret.y;
    } else if (caret.getBottom() > area.getBottom()) {
        scroll.y += caret.getBottom() - area.getBottom();
    }
    scroll.y = std::clamp(scroll.y, 0.0F, std::max(0.0F, size.y - area.height));
}

int TextEditor::callback(ImGuiInputTextCallbackData* data) {
    const Editing& editing = *static_cast<const Editing*>(data->UserData);
    if (data->EventFlag != ImGuiInputTextFlags_CallbackEdit) {
        placeCaret(*data, editing);
    }
    editing.session->synchronize(*data);
    return 0;
}

// ImGui places the caret by its own glyphs, which know no shaping or direction, so the caret moves again here by the shaped text: a press puts it at the character under the pointer, a second press selects the word there, a drag selects up to the pointer, and the arrow keys move it one character on screen. Word jumps with a modifier key keep the reading order of ImGui.
void TextEditor::placeCaret(ImGuiInputTextCallbackData& data, const Editing& editing) {
    const std::string_view buffer(data.Buf, static_cast<std::size_t>(data.BufTextLen));
    const TextFieldLayout field = layOut(*editing.context, buffer, editing.style, editing.password);
    const math::Vec2 origin = getOrigin(field, editing.area, editing.scroll, editing.rightToLeft, editing.multiline);
    const ImGuiIO& io = ImGui::GetIO();
    // clang-format off
    const auto toBytes = [buffer](std::size_t position) {
        return static_cast<int>(core::Utf8::getOffset(buffer, position));
    };
    const auto toPosition = [buffer](int bytes) {
        return core::Utf8::countCodePoints(buffer.substr(0, static_cast<std::size_t>(std::clamp(bytes, 0, static_cast<int>(buffer.size())))));
    };
    const auto select = [&data](int anchor, int caret) {
        data.SelectionStart = anchor;
        data.SelectionEnd = caret;
        data.CursorPos = caret;
    };
    // clang-format on

    const math::Vec2 pointer{io.MousePos.x, io.MousePos.y};
    if (ImGui::IsMouseClicked(0) && editing.bounds.contains(pointer)) {
        const int caret = toBytes(field.hitTest(pointer - origin));
        if (io.MouseClickedCount[0] == 2) {
            const auto [begin, end] = field.getWord(toPosition(caret));
            select(toBytes(begin), toBytes(end));
            return;
        }
        select(io.KeyShift && editing.anchor >= 0 ? editing.anchor : caret, caret);
        return;
    }
    if (ImGui::IsMouseDragging(0) && editing.bounds.contains(math::Vec2{io.MouseClickedPos[0].x, io.MouseClickedPos[0].y}) && io.MouseClickedCount[0] == 1) {
        select(data.SelectionStart, toBytes(field.hitTest(pointer - origin)));
        return;
    }

    if (editing.cursor < 0 || io.KeyCtrl || io.KeyAlt || io.KeySuper) {
        return;
    }
    const bool left = ImGui::IsKeyPressed(ImGuiKey_LeftArrow);
    const bool right = ImGui::IsKeyPressed(ImGuiKey_RightArrow);
    const bool up = editing.multiline && ImGui::IsKeyPressed(ImGuiKey_UpArrow);
    const bool down = editing.multiline && ImGui::IsKeyPressed(ImGuiKey_DownArrow);
    if (left == right && up == down) {
        return;
    }
    const std::size_t from = toPosition(editing.cursor);
    const int caret = toBytes(left != right ? field.moveAcross(from, right) : field.moveAlong(from, down));
    select(io.KeyShift ? editing.anchor : caret, caret);
}

TextEditor::Result TextEditor::draw(Context& context, const math::Rect& bounds, std::string& value, const Options& options) {
    const ImGuiID id = ImGui::GetID("##field");
    const bool focused = ImGui::GetActiveID() == id;
    const bool password = options.input.keyboard == platform::TextInput::Keyboard::Password;
    const bool multiline = options.input.keyboard == platform::TextInput::Keyboard::Multiline;
    TextSession& session = context.getBackend().getTextSession();
    session.addField(id, bounds, options.input);
    Surfaces::draw(context, focused ? Theme::Surface::FieldFocused : Theme::Surface::Field, bounds, context.getColor(Theme::Color::Raised), context.getColor(focused ? Theme::Color::Focus : Theme::Color::Border));

    const math::Rect padded = bounds.inset(Surfaces::getPadding(context, focused ? Theme::Surface::FieldFocused : Theme::Surface::Field));
    const math::Rect inner = context.mirror(math::Rect::fromMinMax({padded.x + options.reserveStart, padded.y}, {std::max(padded.x + options.reserveStart, padded.getRight() - options.reserveEnd), padded.getBottom()}), padded);
    const float paddingX = context.getMetric(Theme::Metric::ControlPaddingX) * 0.5F;
    const float fontSize = context.getFontSize(Theme::Font::Body);
    const float paddingY = multiline ? context.getMetric(Theme::Metric::ControlPaddingY) : std::max(0.0F, (inner.height - fontSize) * 0.5F);
    const math::Rect area = multiline ? inner.inset({paddingX, paddingY, paddingX, paddingY}) : inner.inset({paddingX, 0.0F, paddingX, 0.0F});
    const bool rightToLeft = context.isRightToLeft();
    const text::TextStyle style = Typography::getStyle(context, Theme::Font::Body);

    // The scroll of the text lives with the field, and the caret and the selection it had start the frame.
    ImGuiStorage& storage = *ImGui::GetStateStorage();
    const ImGuiID scrollX = ImGui::GetID("##scrollX");
    const ImGuiID scrollY = ImGui::GetID("##scrollY");
    Editing editing{.session = &session, .context = &context, .style = style, .bounds = bounds, .area = area, .scroll = {storage.GetFloat(scrollX), storage.GetFloat(scrollY)}, .rightToLeft = rightToLeft, .password = password, .multiline = multiline};
    if (const ImGuiInputTextState* state = ImGui::GetInputTextState(id); focused && state != nullptr) {
        editing.cursor = state->GetCursorPos();
        editing.anchor = state->GetSelectionStart();
    }

    // ImGui edits the text while the field draws it, so everything ImGui would draw of it is transparent.
    const ImVec4 hidden(0.0F, 0.0F, 0.0F, 0.0F);
    ImGui::SetCursorScreenPos(ImGuiConverter::toImVec2(inner.getMin()));
    ImGui::PushFont(context.getFont(Theme::Font::Body), fontSize);
    for (const ImGuiCol color : {ImGuiCol_FrameBg, ImGuiCol_Text, ImGuiCol_TextDisabled, ImGuiCol_TextSelectedBg, ImGuiCol_InputTextCursor, ImGuiCol_NavCursor}) {
        ImGui::PushStyleColor(color, hidden);
    }
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
        (void)ImGui::InputTextMultiline("##field", &value, ImGuiConverter::toImVec2(inner.getSize()), flags, &callback, &editing);
    } else {
        ImGui::SetNextItemWidth(inner.width);
        submitted = ImGui::InputText("##field", &value, flags | ImGuiInputTextFlags_EnterReturnsTrue, &callback, &editing);
    }

    // A submit, cancel or dismiss of the platform ends the editing, and only a submit reports the text.
    if (const std::optional<platform::TextInput::Action> action = session.takeAction(id)) {
        submitted = submitted || action == platform::TextInput::Action::Submit;
        ImGui::ClearActiveID();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(6);
    ImGui::PopFont();
    limit(value, options.input.maxLength);

    // The field draws its text, selection and caret from the shaped layout, scrolled so the caret stays in view.
    const TextFieldLayout field = layOut(context, value, style, password);
    const ImGuiInputTextState* state = ImGui::GetActiveID() == id ? ImGui::GetInputTextState(id) : nullptr;
    const auto toPosition = [&value](int bytes) { return core::Utf8::countCodePoints(std::string_view(value).substr(0, static_cast<std::size_t>(std::clamp(bytes, 0, static_cast<int>(value.size()))))); };
    math::Vec2 scroll = editing.scroll;
    if (multiline && ImGui::IsMouseHoveringRect(ImGuiConverter::toImVec2(inner.getMin()), ImGuiConverter::toImVec2(inner.getMax()))) {
        scroll.y = std::clamp(scroll.y - ImGui::GetIO().MouseWheel * fontSize * 3.0F, 0.0F, std::max(0.0F, field.getLayout().size.y - area.height));
    }
    if (state != nullptr) {
        follow(scroll, field, field.getCaret(toPosition(state->GetCursorPos())).translated(getOrigin(field, area, scroll, rightToLeft, multiline)), area, rightToLeft, multiline);
    } else if (!multiline) {
        scroll = {};
    }
    storage.SetFloat(scrollX, scroll.x);
    storage.SetFloat(scrollY, scroll.y);
    const math::Vec2 origin = getOrigin(field, area, scroll, rightToLeft, multiline);

    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.PushClipRect(ImGuiConverter::toImVec2(inner.getMin()), ImGuiConverter::toImVec2(inner.getMax()), true);
    if (state != nullptr && state->HasSelection()) {
        const std::size_t first = toPosition(std::min(state->GetSelectionStart(), state->GetSelectionEnd()));
        const std::size_t last = toPosition(std::max(state->GetSelectionStart(), state->GetSelectionEnd()));
        for (const math::Rect& box : field.getSelection(first, last)) {
            const math::Rect placed = box.translated(origin);
            list.AddRectFilled(ImGuiConverter::toImVec2(placed.getMin()), ImGuiConverter::toImVec2(placed.getMax()), ImGuiConverter::toImU32(context.getColor(Theme::Color::Selection)));
        }
    }

    // An empty field shows its placeholder where its text would start.
    text::TextStyle ink = style;
    if (value.empty()) {
        ink.color = context.getColor(Theme::Color::TextMuted);
        const math::Vec2 size = Typography::layout(context, Theme::Font::Body, options.placeholder, ink)->size;
        Typography::drawLayout(context, Theme::Font::Body, options.placeholder, ink, {rightToLeft ? area.getRight() - size.x : area.x, multiline ? area.y : area.y + (area.height - size.y) * 0.5F});
    } else {
        ink.color = context.getColor(Theme::Color::Text);
        Typography::drawLayout(context, Theme::Font::Body, getShown(value, password), ink, origin);
    }

    // The caret blinks like the one of ImGui, and the platform shows its input method at it.
    if (state != nullptr) {
        const math::Rect caret = field.getCaret(toPosition(state->GetCursorPos())).translated(origin);
        const bool shown = !ImGui::GetIO().ConfigInputTextCursorBlink || state->CursorAnim <= 0.0F || std::fmod(state->CursorAnim, 1.20F) <= 0.80F;
        if (shown) {
            const float width = context.getMetric(Theme::Metric::CaretWidth);
            list.AddRectFilled({caret.x - width * 0.5F, caret.y}, {caret.x + width * 0.5F, caret.getBottom()}, ImGuiConverter::toImU32(context.getColor(Theme::Color::Text)));
        }
        session.setCaret({caret.x, caret.y, 1.0F, caret.height});
        if (!password) {
            drawComposition(session, id, field, origin);
        }
    }
    list.PopClipRect();

    context.getFocus().addTarget(id, bounds);
    Widgets::drawFocusRing(context, bounds, id, context.getMetric(Theme::Metric::ControlRadius));
    return {.changed = value != before, .submitted = submitted};
}

void TextEditor::drawComposition(const TextSession& session, ImGuiID id, const TextFieldLayout& field, math::Vec2 origin) {
    const std::optional<std::pair<int, int>> composition = session.getComposition(id);
    if (!composition) {
        return;
    }
    const float thickness = std::max(1.0F, ImGui::GetFontSize() / 14.0F);
    ImDrawList& list = *ImGui::GetWindowDrawList();
    for (const math::Rect& box : field.getSelection(static_cast<std::size_t>(composition->first), static_cast<std::size_t>(composition->second))) {
        const math::Rect placed = box.translated(origin);
        list.AddLine({placed.x, placed.getBottom() - 1.0F}, {placed.getRight(), placed.getBottom() - 1.0F}, ImGui::GetColorU32(ImGuiCol_Text), thickness);
    }
}

// A length limit counts code points and never cuts a character in half.
void TextEditor::limit(std::string& value, int maxLength) {
    if (maxLength > 0) {
        value.resize(core::Utf8::getOffset(value, static_cast<std::size_t>(maxLength)));
    }
}

} // namespace haylen::ui
