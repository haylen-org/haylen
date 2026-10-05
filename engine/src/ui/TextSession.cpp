#include "ui/TextSession.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

#include "haylen/core/Utf8.hpp"

namespace haylen::ui {

std::uint64_t TextSession::lastRevision = 0;

bool TextSession::limit(ImGuiInputTextCallbackData& data, int maxLength) {
    if (maxLength <= 0) {
        return false;
    }
    const std::string_view buffer(data.Buf, static_cast<std::size_t>(data.BufTextLen));
    const std::size_t end = core::Utf8::getOffset(buffer, static_cast<std::size_t>(maxLength));
    if (end >= buffer.size()) {
        return false;
    }
    data.DeleteChars(static_cast<int>(end), data.BufTextLen - static_cast<int>(end));
    return true;
}

void TextSession::apply(ImGuiInputTextCallbackData& data, const platform::TextInput::Edit& edit) {
    if (std::string_view(data.Buf, static_cast<std::size_t>(data.BufTextLen)) != edit.text) {
        data.DeleteChars(0, data.BufTextLen);
        if (!edit.text.empty()) {
            data.InsertChars(0, edit.text.data(), edit.text.data() + edit.text.size());
        }
    }

    const std::string_view buffer(data.Buf, static_cast<std::size_t>(data.BufTextLen));
    data.SelectionStart = static_cast<int>(core::Utf8::getOffset(buffer, static_cast<std::size_t>(std::max(0, edit.selectionStart))));
    data.SelectionEnd = static_cast<int>(core::Utf8::getOffset(buffer, static_cast<std::size_t>(std::max(0, edit.selectionEnd))));
    data.CursorPos = data.SelectionEnd;
}

std::pair<int, int> TextSession::getSelection(const ImGuiInputTextCallbackData& data) {
    const std::string_view buffer(data.Buf, static_cast<std::size_t>(data.BufTextLen));
    // clang-format off
    const auto toIndex = [buffer](int offset) {
        return static_cast<int>(core::Utf8::countCodePoints(buffer.substr(0, static_cast<std::size_t>(std::clamp(offset, 0, static_cast<int>(buffer.size()))))));
    };
    // clang-format on
    if (data.SelectionStart == data.SelectionEnd) {
        const int caret = toIndex(data.CursorPos);
        return {caret, caret};
    }
    return {toIndex(std::min(data.SelectionStart, data.SelectionEnd)), toIndex(std::max(data.SelectionStart, data.SelectionEnd))};
}

void TextSession::receiveEdit(platform::TextInput::Edit edit) {
    if (active != 0 && edit.field == active) {
        pending = std::move(edit);
    }
}

void TextSession::beginFrame(const graphics::Viewport& frameViewport, float uiScale, float deltaSeconds) {
    viewport = frameViewport;
    origin = frameViewport.getVisibleRect().getMin();
    scale = uiScale;
    fields.clear();
    activeThisFrame = false;

    // The field was drawn moved by the offset of the last frame, so its place without the offset decides how far the UI must move, and the field never leaves the top of the screen.
    float target = 0.0F;
    if (active != 0 && !keyboard.isEmpty()) {
        const float top = bounds.getTop() + keyboardOffset;
        const float overlap = bounds.getBottom() + keyboardOffset + kKeyboardMargin - keyboard.getTop();
        target = std::clamp(overlap, 0.0F, std::max(0.0F, top - kKeyboardMargin));
    }
    keyboardOffset += (target - keyboardOffset) * std::min(1.0F, deltaSeconds * kKeyboardSpeed);
    if (std::abs(target - keyboardOffset) < 0.5F) {
        keyboardOffset = target;
    }
}

void TextSession::addField(ImGuiID id, const math::Rect& fieldBounds, const platform::TextInput::Options& fieldOptions) {
    fields.push_back({.id = id, .bounds = fieldBounds, .options = fieldOptions});
}

void TextSession::synchronize(ImGuiInputTextCallbackData& data) {
    const auto entry = std::ranges::find(fields, data.ID, &Entry::id);
    if (entry == fields.end()) {
        return;
    }
    const bool activated = data.EventActivated || data.ID != active;
    if (activated) {
        active = data.ID;
        revision = ++lastRevision;
        pending.reset();
        action.reset();
        compositionStart = -1;
        compositionEnd = -1;
        caretRect = entry->bounds;
        original.assign(data.Buf, static_cast<std::size_t>(data.BufTextLen));
    }
    options = entry->options;
    bounds = entry->bounds;
    activeThisFrame = true;

    // An edit made against an older revision, or in a frame where ImGui itself changed the text, answers a text the field no longer shows.
    const bool edited = data.EventFlag == ImGuiInputTextFlags_CallbackEdit;
    bool applied = false;
    if (pending) {
        if (!activated && !edited && pending->revision == revision) {
            apply(data, *pending);
            compositionStart = pending->compositionStart;
            compositionEnd = pending->compositionEnd;
            applied = true;
        }
        pending.reset();
    }

    // Cancelling brings back the text the field had when it took the focus, before its editor lets it go.
    if (action == platform::TextInput::Action::Cancel) {
        const auto end = static_cast<int>(core::Utf8::countCodePoints(original));
        apply(data, {.text = original, .selectionStart = end, .selectionEnd = end});
        applied = true;
    }

    // The UI changed the field when ImGui edited it, when the limit cut it, or when the caret moved without an edit of the platform, as a click in the field moves it.
    const bool cut = limit(data, options.maxLength);
    std::string current(data.Buf, static_cast<std::size_t>(data.BufTextLen));
    const auto [start, end] = getSelection(data);
    const bool changed = edited || cut || (!applied && (current != text || start != selectionStart || end != selectionEnd));
    if (changed && !activated) {
        revision = ++lastRevision;
        compositionStart = -1;
        compositionEnd = -1;
    }
    text = std::move(current);
    selectionStart = start;
    selectionEnd = end;
}

std::optional<platform::TextInput::Action> TextSession::takeAction(ImGuiID id) {
    if (id != active) {
        return std::nullopt;
    }
    return std::exchange(action, std::nullopt);
}

std::optional<std::pair<int, int>> TextSession::getComposition(ImGuiID id) const {
    if (id != active || compositionStart < 0 || compositionEnd <= compositionStart) {
        return std::nullopt;
    }
    return std::pair{compositionStart, compositionEnd};
}

void TextSession::publish() {
    const bool composing = compositionStart >= 0 && compositionEnd > compositionStart;
    if (!activeThisFrame) {
        active = 0;
        pending.reset();
        action.reset();
        compositionStart = -1;
        compositionEnd = -1;
        if (published) {
            published.reset();
            input.finish();
        }
    } else if (!composing) {
        platform::TextInput::Field field{.id = active, .revision = revision, .text = text, .selectionStart = selectionStart, .selectionEnd = selectionEnd, .bounds = toFramebuffer(bounds), .caret = toFramebuffer(caretRect), .options = options};
        if (published != field) {
            input.edit(field);
            published = std::move(field);
        }
    }
    publishVisibleFields();
}

math::Rect TextSession::toFramebuffer(const math::Rect& rect) const noexcept {
    return math::Rect::fromMinMax(viewport.toFramebuffer(rect.getMin() * scale + origin), viewport.toFramebuffer(rect.getMax() * scale + origin));
}

// The list is rebuilt every frame in storage kept from the frames before, and the platform hears it only when it changes.
void TextSession::publishVisibleFields() {
    visibleFields.clear();
    for (const Entry& entry : fields) {
        visibleFields.push_back({.id = entry.id, .bounds = toFramebuffer(entry.bounds), .options = entry.options});
    }
    if (visibleFields != sentFields) {
        input.setVisibleFields(visibleFields);
        sentFields = visibleFields;
    }
}

} // namespace haylen::ui
