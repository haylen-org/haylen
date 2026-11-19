#include "ui/TextEditor.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "haylen/core/Utf8.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/ScrollFade.hpp"
#include "ui/Scrollbar.hpp"
#include "ui/Surfaces.hpp"
#include "ui/TextSession.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

std::string_view TextEditor::writeBullets(std::string_view value, std::string& bullets) {
    bullets.clear();
    for (std::size_t index = core::Utf8::countCodePoints(value); index > 0; --index) {
        bullets += kBullet;
    }

    return bullets;
}

TextFieldLayout TextEditor::layOut(Context& context, std::string_view shown, const text::Style& style) {
    return {Typography::layout(context, Theme::Font::Body, shown, style), context.getFrameCodePoints(shown)};
}

math::Vec2 TextEditor::getOrigin(const text::Layout& laid, const math::Rect& area, math::Vec2 scroll, bool rightToLeft, bool multiline, bool centered) {
    const math::Vec2 size = laid.size;
    float x = rightToLeft ? area.getRight() - size.x : area.x;
    if (isCentered(size, area, multiline, centered)) {
        x = area.x + (area.width - size.x) * 0.5F;
    }

    const float y = multiline ? area.y : area.y + (area.height - size.y) * 0.5F;
    return {x + scroll.x, y - scroll.y};
}

bool TextEditor::isCentered(math::Vec2 size, const math::Rect& area, bool multiline, bool centered) noexcept {
    return centered && !multiline && size.x + kCaretMargin * 2.0F <= area.width;
}

std::size_t TextEditor::toPosition(std::string_view text, int bytes) {
    const auto clamped = static_cast<std::size_t>(std::clamp(bytes, 0, static_cast<int>(text.size())));
    return core::Utf8::countCodePoints(text.substr(0, clamped));
}

// The lines of an editor of many lines wrap at the width of its area, so its text only scrolls down, while a field of one line scrolls to the left for left-to-right text and to the right for right-to-left text, and never further than its overflow.
void TextEditor::follow(math::Vec2& scroll, const TextFieldLayout& field, const math::Rect& caret, const math::Rect& area, bool rightToLeft, bool multiline) {
    const math::Vec2 size = field.getLayout().size;
    if (multiline) {
        if (caret.y < area.y) {
            scroll.y -= area.y - caret.y;
        } else if (caret.getBottom() > area.getBottom()) {
            scroll.y += caret.getBottom() - area.getBottom();
        }

        scroll = {0.0F, std::clamp(scroll.y, 0.0F, std::max(0.0F, size.y - area.height))};
        return;
    }

    if (caret.x > area.getRight() - kCaretMargin) {
        scroll.x -= caret.x - (area.getRight() - kCaretMargin);
    } else if (caret.x < area.x + kCaretMargin) {
        scroll.x += area.x + kCaretMargin - caret.x;
    }

    const float overflow = std::max(0.0F, size.x + kCaretMargin * 2.0F - area.width);
    scroll.x = rightToLeft ? std::clamp(scroll.x, 0.0F, overflow) : std::clamp(scroll.x, -overflow, 0.0F);
}

int TextEditor::callback(ImGuiInputTextCallbackData* data) {
    const Editing& editing = *static_cast<const Editing*>(data->UserData);
    if (data->EventFlag != ImGuiInputTextFlags_CallbackEdit) {
        placeCaret(*data, editing);
    }

    editing.session->synchronize(*data);
    return 0;
}

// ImGui places the caret by its own glyphs, which know no shaping or direction, so the caret moves again here by the shaped text: a press, or the tap of a finger that began the editing, puts it at the character under the pointer, a second press selects the word there, a drag of the mouse selects up to the pointer, and the arrow keys move it one character on screen. Word jumps with a modifier key keep the reading order of ImGui.
void TextEditor::placeCaret(ImGuiInputTextCallbackData& data, const Editing& editing) {
    const ImGuiIO& io = ImGui::GetIO();
    const math::Vec2 pointer{io.MousePos.x, io.MousePos.y};
    const math::Vec2 point = editing.tap.value_or(pointer);
    const bool pressed = editing.tap.has_value() || (ImGui::IsMouseClicked(0) && editing.bounds.contains(pointer));
    const math::Vec2 pressPoint{io.MouseClickedPos[0].x, io.MouseClickedPos[0].y};
    const bool dragged = io.MouseSource != ImGuiMouseSource_TouchScreen && ImGui::IsMouseDragging(0) && editing.bounds.contains(pressPoint) && io.MouseClickedCount[0] == 1;
    const bool arrows = editing.cursor >= 0 && !io.KeyCtrl && !io.KeyAlt && !io.KeySuper;
    const bool left = arrows && ImGui::IsKeyPressed(ImGuiKey_LeftArrow);
    const bool right = arrows && ImGui::IsKeyPressed(ImGuiKey_RightArrow);
    const bool up = arrows && editing.multiline && ImGui::IsKeyPressed(ImGuiKey_UpArrow);
    const bool down = arrows && editing.multiline && ImGui::IsKeyPressed(ImGuiKey_DownArrow);
    if (!pressed && !dragged && left == right && up == down) {
        return;
    }

    // Only a press, a drag or an arrow key lays the text out, so a focused field that nobody touches costs nothing.
    const std::string_view buffer(data.Buf, static_cast<std::size_t>(data.BufTextLen));
    std::string bullets;
    const TextFieldLayout field = layOut(*editing.context, editing.password ? writeBullets(buffer, bullets) : buffer, editing.style);
    const math::Vec2 origin = getOrigin(field.getLayout(), editing.area, editing.scroll, editing.rightToLeft, editing.multiline, editing.centered);
    // clang-format off
    const auto toBytes = [buffer](std::size_t position) {
        return static_cast<int>(core::Utf8::getOffset(buffer, position));
    };
    const auto select = [&data](int anchor, int caret) {
        data.SelectionStart = anchor;
        data.SelectionEnd = caret;
        data.CursorPos = caret;
    };
    // clang-format on

    if (pressed) {
        editing.session->notePress();
        const int caret = toBytes(field.hitTest(point - origin));
        if (!editing.tap && io.MouseClickedCount[0] == 2) {
            const auto [begin, end] = field.getWord(toPosition(buffer, caret));
            select(toBytes(begin), toBytes(end));
            return;
        }

        select(io.KeyShift && editing.anchor >= 0 ? editing.anchor : caret, caret);
        return;
    }

    if (dragged) {
        select(data.SelectionStart, toBytes(field.hitTest(pointer - origin)));
        return;
    }

    const std::size_t from = toPosition(buffer, editing.cursor);
    const int caret = toBytes(left != right ? field.moveAcross(from, right) : field.moveAlong(from, down));
    select(io.KeyShift ? editing.anchor : caret, caret);
}

// The bar keeps inside the frame of the field, and the text and the ImGui field leave its lane.
TextEditor::Geometry TextEditor::place(Context& context, const math::Rect& bounds, Theme::Surface surface, const Options& options, bool multiline, bool barShown) {
    Geometry geometry;
    geometry.frame = Surfaces::getFrame(context, surface);
    geometry.barArea = bounds.inset(geometry.frame);

    const math::Rect padded = bounds.inset(Surfaces::getPadding(context, surface));
    const float wholeStart = padded.x + options.reserveStart;
    const math::Rect reserved = math::Rect::fromMinMax({wholeStart, padded.y}, {std::max(wholeStart, padded.getRight() - options.reserveEnd), padded.getBottom()});
    const math::Rect whole = context.mirror(reserved, padded);
    geometry.inner = barShown ? Scrollbar::getContentBox(context, geometry.barArea, whole, false) : whole;

    // An editor of many lines keeps the padding of a control above its first line, and a field of one line centers its line.
    geometry.paddingX = context.getMetric(Theme::Metric::ControlPaddingX) * 0.5F;
    geometry.fontSize = context.getFontHeight(Theme::Font::Body);
    geometry.paddingY = multiline ? context.getMetric(Theme::Metric::ControlPaddingY) : std::max(0.0F, (geometry.inner.height - geometry.fontSize) * 0.5F);
    const float spacingY = multiline ? geometry.paddingY : 0.0F;
    const math::Rect spaced = whole.inset({geometry.paddingX, spacingY, geometry.paddingX, spacingY});
    geometry.area = barShown ? Scrollbar::getContentBox(context, geometry.barArea, spaced, false) : spaced;
    return geometry;
}

// ImGui edits the text while the field draws it, so everything ImGui would draw of it is transparent. A finger edits a field only once it taps it, as on a phone, so while a finger presses a field that is not editing the field waits as a plain button, which a drag lets go of for the area that scrolls, and a tap starts the editing in the next frame with the caret where the finger lifted.
TextEditor::Input TextEditor::edit(Context& context, std::string& value, const Options& options, const Geometry& geometry, Editing& editing, ImGuiID id, bool focused) {
    const ImVec4 hidden(0.0F, 0.0F, 0.0F, 0.0F);
    ImGui::SetCursorScreenPos(ImGuiConverter::toImVec2(geometry.inner.getMin()));
    ImGui::PushFont(context.getFont(Theme::Font::Body), geometry.fontSize);

    for (const ImGuiCol color : {ImGuiCol_FrameBg, ImGuiCol_Text, ImGuiCol_TextDisabled, ImGuiCol_TextSelectedBg, ImGuiCol_InputTextCursor, ImGuiCol_NavCursor}) {
        ImGui::PushStyleColor(color, hidden);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {geometry.paddingX, geometry.paddingY});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0F);

    ImGuiStorage& storage = *ImGui::GetStateStorage();
    const ImGuiIO& io = ImGui::GetIO();
    const ImGuiID tapped = ImGui::GetID("##tapped");
    const ImGuiID tapX = ImGui::GetID("##tapX");
    const ImGuiID tapY = ImGui::GetID("##tapY");
    const math::Vec2 pressPoint{io.MouseClickedPos[0].x, io.MouseClickedPos[0].y};
    const bool fingerPress = !focused && io.MouseSource == ImGuiMouseSource_TouchScreen && (io.MouseDown[0] || io.MouseReleased[0]) && editing.bounds.contains(pressPoint);

    // The focus reaches the field a frame or two after it is asked for, so the tap waits until the field edits and places the caret in the frame the editing starts.
    if (storage.GetBool(tapped) && focused) {
        storage.SetBool(tapped, false);
    } else if (storage.GetBool(tapped)) {
        editing.tap = math::Vec2{storage.GetFloat(tapX), storage.GetFloat(tapY)};
        ImGui::SetKeyboardFocusHere();
    } else if (options.focus) {
        ImGui::SetKeyboardFocusHere();
    }

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackEdit;
    if (editing.password) {
        flags |= ImGuiInputTextFlags_Password;
    }

    Input input;
    if (fingerPress) {
        if (ImGui::InvisibleButton("##press", ImGuiConverter::toImVec2(geometry.inner.getSize()))) {
            storage.SetBool(tapped, true);
            storage.SetFloat(tapX, io.MousePos.x);
            storage.SetFloat(tapY, io.MousePos.y);
        }
    } else if (editing.multiline) {
        static_cast<void>(ImGui::InputTextMultiline("##field", &value, ImGuiConverter::toImVec2(geometry.inner.getSize()), flags, &callback, &editing));
    } else {
        ImGui::SetNextItemWidth(geometry.inner.width);
        input.submitted = ImGui::InputText("##field", &value, flags | ImGuiInputTextFlags_EnterReturnsTrue, &callback, &editing);
    }

    // The selection that a press drags keeps its finger, while the press of a field that waits for a tap goes once the finger travels.
    input.edited = ImGui::IsItemEdited();
    if (!fingerPress && ImGui::IsItemActive()) {
        context.keepDrag(ImGui::GetItemID());
    }

    // A submit, cancel or dismiss of the platform ends the editing, and only a submit reports the text.
    if (const std::optional<platform::TextInput::Action> action = editing.session->takeAction(id)) {
        input.submitted = input.submitted || action == platform::TextInput::Action::Submit;
        ImGui::ClearActiveID();
    }

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(6);
    ImGui::PopFont();
    return input;
}

// The wheel and a finger scroll the lines of an editor of many lines, the scroll then keeps the caret of a focused field in view, and the scroll bar moves the lines last. A field of one line that is not focused shows the start of its text.
math::Vec2 TextEditor::scrollText(const Editing& editing, const Options& options, const Geometry& geometry, const TextFieldLayout* field, std::optional<std::size_t> caret, ImGuiID id, float overflow, bool barShown) {
    Context& context = *editing.context;
    math::Vec2 scroll = editing.scroll;
    if (editing.multiline) {
        const bool hovered = ImGui::IsMouseHoveringRect(ImGuiConverter::toImVec2(editing.bounds.getMin()), ImGuiConverter::toImVec2(editing.bounds.getMax()));
        scroll.y = static_cast<float>(options.scrolling->lines.update(context, id, editing.bounds, overflow, geometry.area.height, hovered));
    }

    const float wheeled = scroll.y;
    if (field != nullptr && caret) {
        const math::Vec2 origin = getOrigin(field->getLayout(), geometry.area, scroll, editing.rightToLeft, editing.multiline, editing.centered);
        const math::Rect shownCaret = field->getCaret(*caret).translated(origin);
        follow(scroll, *field, shownCaret, geometry.area, editing.rightToLeft, editing.multiline);
    } else if (!editing.multiline) {
        scroll = {};
    }

    if (barShown) {
        const float radius = Surfaces::getInnerRadius(context.getMetric(Theme::Metric::ControlRadius), editing.rightToLeft ? geometry.frame.left : geometry.frame.right);
        if (const std::optional<double> offset = options.scrolling->bar.interact(context, geometry.barArea, false, false, scroll.y, overflow, geometry.area.height, radius)) {
            scroll.y = static_cast<float>(*offset);
        }
    }

    // The caret and the bar move the lines at once.
    if (editing.multiline && scroll.y != wheeled) {
        options.scrolling->lines.jumpTo(scroll.y);
    }

    return scroll;
}

// An empty field shows its placeholder where its text would start, and returns the edge where the placeholder starts reading.
std::optional<float> TextEditor::drawValue(const Editing& editing, std::string_view shown, std::string_view placeholder, math::Vec2 origin) {
    Context& context = *editing.context;
    text::Style ink = editing.style;
    if (!shown.empty()) {
        ink.color = context.getColor(Theme::Color::Text);
        Typography::drawLayout(context, Theme::Font::Body, shown, ink, origin);
        return std::nullopt;
    }

    ink.color = context.getColor(Theme::Color::TextMuted);
    const std::shared_ptr<const text::Layout> laid = Typography::layout(context, Theme::Font::Body, placeholder, ink);
    const math::Vec2 placeholderOrigin = getOrigin(*laid, editing.area, {}, editing.rightToLeft, editing.multiline, editing.centered);
    const math::Vec2 start{std::floor(placeholderOrigin.x), placeholderOrigin.y};
    Typography::drawLayout(context, Theme::Font::Body, placeholder, ink, start);

    if (placeholder.empty()) {
        return std::nullopt;
    }

    return editing.rightToLeft ? start.x + laid->size.x : start.x;
}

void TextEditor::drawSelection(Context& context, std::string_view value, const ImGuiInputTextState& state, const TextFieldLayout& field, math::Vec2 origin) {
    if (!state.HasSelection()) {
        return;
    }

    const std::size_t first = toPosition(value, std::min(state.GetSelectionStart(), state.GetSelectionEnd()));
    const std::size_t last = toPosition(value, std::max(state.GetSelectionStart(), state.GetSelectionEnd()));
    std::vector<math::Rect>& boxes = context.getSelectionBoxes();
    field.getSelection(first, last, boxes);
    for (const math::Rect& box : boxes) {
        Surfaces::fill(context, box.translated(origin), context.getColor(Theme::Color::Selection), 0.0F);
    }
}

// The caret blinks like the one of ImGui, and the platform shows its input method at it. The caret of an empty field stands before its placeholder with a caret width of room between them, so it never touches the first letter.
void TextEditor::drawCaret(const Editing& editing, const ImGuiInputTextState& state, const TextFieldLayout& field, std::size_t position, math::Vec2 origin, std::optional<float> placeholderEdge) {
    Context& context = *editing.context;
    const float width = context.getMetric(Theme::Metric::CaretWidth);
    math::Rect caret = field.getCaret(position).translated(origin);
    if (placeholderEdge) {
        caret.x = *placeholderEdge + (editing.rightToLeft ? width * 1.5F : -width * 1.5F);
    }

    const bool lit = !ImGui::GetIO().ConfigInputTextCursorBlink || state.CursorAnim <= 0.0F || std::fmod(state.CursorAnim, 1.20F) <= 0.80F;
    if (lit) {
        Surfaces::fill(context, {caret.x - width * 0.5F, caret.y, width, caret.height}, context.getColor(Theme::Color::Caret), 0.0F);
    }

    editing.session->setCaret({caret.x, caret.y, 1.0F, caret.height});
}

TextEditor::Result TextEditor::draw(Context& context, const math::Rect& bounds, std::string& value, const Options& options) {
    const ImGuiID id = ImGui::GetID("##field");
    const bool focused = ImGui::GetActiveID() == id;
    const bool password = options.input.keyboard == platform::TextInput::Keyboard::Password;
    const bool multiline = options.input.keyboard == platform::TextInput::Keyboard::Multiline;
    TextSession& session = context.getBackend().getTextSession();
    session.addField(id, bounds, options.input);

    const Theme::Surface surface = focused ? Theme::Surface::FieldFocused : Theme::Surface::Field;
    Surfaces::draw(context, surface, bounds, context.getColor(Theme::Color::Raised), context.getColor(focused ? Theme::Color::Focus : Theme::Color::Border));

    // The horizontal scroll of the text lives with the field, and so does whether its text was taller than its area in the last frame, which shows the scroll bar that an editor of many lines asks for from the frame after the text grew, so the area never changes while the text lays out.
    ImGuiStorage& storage = *ImGui::GetStateStorage();
    const ImGuiID scrollX = ImGui::GetID("##scrollX");
    const ImGuiID overflowed = ImGui::GetID("##overflowed");
    const bool barShown = multiline && options.scrolling->barWanted && storage.GetBool(overflowed);
    const Geometry geometry = place(context, bounds, surface, options, multiline, barShown);

    // Text being typed fills its lines as far as they go, so the lines before the caret keep their words as it moves on.
    text::Style style = Typography::getStyle(context, Theme::Font::Body, multiline ? std::max(1.0F, geometry.area.width) : -1.0F);
    style.balance = text::Balance::None;

    // The caret and the selection the field had start the frame.
    const float storedLines = multiline ? static_cast<float>(std::max(0.0, options.scrolling->lines.getOffset())) : 0.0F;
    const math::Vec2 storedScroll{storage.GetFloat(scrollX), storedLines};
    Editing editing{.session = &session, .context = &context, .style = style, .bounds = bounds, .area = geometry.area, .scroll = storedScroll, .rightToLeft = context.isRightToLeft(), .password = password, .multiline = multiline, .centered = options.centered};

    // ImGui leaves the start of the last selection behind when the caret moves without selecting, so only a selection anchors a shift press.
    if (const ImGuiInputTextState* stored = ImGui::GetInputTextState(id); focused && stored != nullptr) {
        editing.cursor = stored->GetCursorPos();
        editing.anchor = stored->HasSelection() ? stored->GetSelectionStart() : editing.cursor;
    }

    const Input input = edit(context, value, options, geometry, editing, id, focused);
    const bool shortened = limit(value, options.input.maxLength);

    // The field draws its text from the shaped layout, scrolled so the caret stays in view, and a focused field also its selection and caret from the code points of the text.
    const std::string_view shown = password ? writeBullets(value, context.getFrameText()) : std::string_view(value);
    const std::shared_ptr<const text::Layout> laid = Typography::layout(context, Theme::Font::Body, shown, style);
    const ImGuiInputTextState* state = ImGui::GetActiveID() == id ? ImGui::GetInputTextState(id) : nullptr;
    std::optional<TextFieldLayout> field;
    std::optional<std::size_t> caret;
    if (state != nullptr) {
        field.emplace(laid, context.getFrameCodePoints(shown));
        caret = toPosition(value, state->GetCursorPos());
    }

    const float overflow = std::max(0.0F, laid->size.y - geometry.area.height);
    const math::Vec2 scroll = scrollText(editing, options, geometry, field ? &*field : nullptr, caret, id, overflow, barShown);
    storage.SetFloat(scrollX, scroll.x);

    if (multiline) {
        storage.SetBool(overflowed, overflow > 0.0F);
    }

    // Lines that continue past the area fade out along its edges, as the content of the other areas that scroll does.
    const math::Vec2 origin = getOrigin(*laid, geometry.area, scroll, editing.rightToLeft, multiline, options.centered);
    context.pushClip(geometry.inner);
    const bool faded = multiline && ScrollFade::begin(context, geometry.inner, false, scroll.y, overflow - scroll.y);

    if (state != nullptr) {
        drawSelection(context, value, *state, *field, origin);
    }

    const std::optional<float> placeholderEdge = drawValue(editing, shown, options.placeholder, origin);

    if (state != nullptr) {
        drawCaret(editing, *state, *field, *caret, origin, placeholderEdge);
    }

    if (state != nullptr && !password) {
        drawComposition(context, session, id, *field, origin);
    }

    if (faded) {
        ScrollFade::end(context);
    }

    Context::popClip();

    if (barShown) {
        options.scrolling->bar.draw(context, 1.0F);
    }

    context.takePointer(bounds);
    context.getFocus().addTarget(id, bounds);
    Widgets::drawFocusRing(context, bounds, id, context.getMetric(Theme::Metric::ControlRadius));
    return {.changed = input.edited || shortened, .submitted = input.submitted};
}

void TextEditor::drawComposition(Context& context, const TextSession& session, ImGuiID id, const TextFieldLayout& field, math::Vec2 origin) {
    const std::optional<std::pair<int, int>> composition = session.getComposition(id);
    if (!composition) {
        return;
    }

    const float thickness = std::max(1.0F, ImGui::GetFontSize() / 14.0F);
    std::vector<math::Rect>& boxes = context.getSelectionBoxes();
    field.getSelection(static_cast<std::size_t>(composition->first), static_cast<std::size_t>(composition->second), boxes);
    for (const math::Rect& box : boxes) {
        const math::Rect placed = box.translated(origin);
        Widgets::line(context, {placed.x, placed.getBottom() - 1.0F}, {placed.getRight(), placed.getBottom() - 1.0F}, thickness, context.getColor(Theme::Color::Text));
    }
}

// A length limit counts code points and never cuts a character in half.
bool TextEditor::limit(std::string& value, int maxLength) {
    if (maxLength <= 0) {
        return false;
    }

    const std::size_t end = core::Utf8::getOffset(value, static_cast<std::size_t>(maxLength));
    if (end >= value.size()) {
        return false;
    }

    value.resize(end);
    return true;
}

} // namespace haylen::ui
