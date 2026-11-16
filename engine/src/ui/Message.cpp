#include "haylen/ui/Message.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/ui/PropertyReader.hpp"
#include "ui/MessageContent.hpp"

namespace haylen::ui {

void Message::Content::read(PropertyReader& reader) {
    reader.read("title", title);
    reader.read("text", text);
    reader.read("action", action);
    reader.read("icon", icon);
    reader.readChoice<Widgets::Tone>("tone", tone, Widgets::kTones);
    reader.readChoice<Variant>("variant", variant, kVariantNames);
    reader.readChoice<Indicator>("indicator", indicator, kIndicatorNames);
    reader.read("progress", progress, 0.0F, 1.0F);
    reader.read("closable", closable);
    if (reader.has("duration")) {
        float seconds = 0.0F;
        reader.read("duration", seconds, 0.0F, 3600.0F);
        duration = seconds;
    }
}

// The style sets where a message stacks before its own properties do: a banner at the top under the status bar, and a toast and a snackbar at the bottom.
Message::Message(const core::Json& properties) : content(std::make_unique<Content>()) {
    checkKeys(properties, true);
    PropertyReader reader(properties, "message");
    reader.readChoice<Style>("style", style, kStyleNames);
    reader.readChoice<Priority>("priority", priority, kPriorityNames);
    content->position = style == Style::Banner ? MessageStack::Position::Top : MessageStack::Position::Bottom;
    reader.readChoice<MessageStack::Position>("position", content->position, MessageStack::kPositions);
    reader.readChoice<MessageStack::Arrangement>("arrangement", content->arrangement, MessageStack::kArrangements);
    reader.read("expandable", content->expandable);
    content->read(reader);
    reader.finish();
    checkText(*content);
}

Message::~Message() = default;

void Message::checkKeys(const core::Json& properties, bool creating) {
    if (!properties.is_object()) {
        throw std::invalid_argument("The properties of a message must be an object.");
    }

    for (const auto& [key, value] : properties.items()) {
        const bool described = std::ranges::find(Content::kContentKeys, key) != Content::kContentKeys.end();
        const bool placed = std::ranges::find(Content::kPlacementKeys, key) != Content::kPlacementKeys.end();
        if (!described && !placed) {
            throw std::invalid_argument("A message has no property \"" + key + "\".");
        }

        if (placed && !creating) {
            throw std::invalid_argument("The property \"" + key + "\" of a message is set when it shows, and an update cannot change it.");
        }
    }
}

void Message::checkText(const Content& checked) {
    if (checked.title.isEmpty() && checked.text.isEmpty()) {
        throw std::invalid_argument("A message needs a \"title\" or a \"text\".");
    }
}

void Message::update(const core::Json& properties) {
    checkKeys(properties, false);
    Content changed = *content;
    PropertyReader reader(properties, "message");
    changed.read(reader);
    reader.finish();
    checkText(changed);

    *content = std::move(changed);
    if (properties.contains("duration")) {
        elapsed = 0.0F;
    }
}

void Message::dismiss() {
    leave(Reason::Dismissed);
}

void Message::leave(Reason why) {
    if (phase == Phase::Leaving || phase == Phase::Gone) {
        return;
    }

    reason = why;
    phase = phase == Phase::Waiting ? Phase::Gone : Phase::Leaving;
}

void Message::vanish() {
    if (phase == Phase::Gone) {
        return;
    }

    if (!reason) {
        reason = Reason::Dismissed;
    }

    phase = Phase::Gone;
}

float Message::getDuration() const noexcept {
    if (content->duration) {
        return *content->duration;
    }

    switch (style) {
    case Style::Snackbar:
        return content->action.isEmpty() ? kSnackbarDuration : kActionDuration;
    case Style::Banner:
        return kBannerDuration;
    case Style::Toast:
        break;
    }

    return kToastDuration;
}

std::string_view Message::styleName(Style value) noexcept {
    return std::ranges::find(kStyleNames, value, &std::pair<std::string_view, Style>::second)->first;
}

std::string_view Message::reasonName(Reason value) noexcept {
    return std::ranges::find(kReasonNames, value, &std::pair<std::string_view, Reason>::second)->first;
}

} // namespace haylen::ui
