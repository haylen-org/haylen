#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/ui/Message.hpp"
#include "haylen/ui/MessageStack.hpp"
#include "haylen/ui/TextValue.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

class PropertyReader;

// What a message says and how it looks: its title over its text, a picture or an indicator at its start, an action and a close button at its end, the colors of its tone and variant, and where it stacks.
struct Message::Content {
    // How a message paints itself: filled with the color of its tone, filled with the pale color of its tone, or on the raised color with a border of its tone.
    enum class Variant : std::uint8_t {
        Solid,
        Soft,
        Outline,
    };

    // What a message shows before its text when it has no picture: nothing, the symbol of its tone, a spinner, or a ring that fills with its progress.
    enum class Indicator : std::uint8_t {
        None,
        Icon,
        Busy,
        Progress,
    };

    static constexpr std::array<std::pair<std::string_view, Variant>, 3> kVariantNames{{{"solid", Variant::Solid}, {"soft", Variant::Soft}, {"outline", Variant::Outline}}};
    static constexpr std::array<std::pair<std::string_view, Indicator>, 4> kIndicatorNames{{{"none", Indicator::None}, {"icon", Indicator::Icon}, {"busy", Indicator::Busy}, {"progress", Indicator::Progress}}};

    // Reads the properties of the content, leaving the others to the reader.
    void read(PropertyReader& reader);

    TextValue title;
    TextValue text;
    TextValue action;

    // The path of the picture at the start of the message, such as the avatar of whoever wrote.
    std::string icon;
    Widgets::Tone tone = Widgets::Tone::Neutral;
    Variant variant = Variant::Solid;
    Indicator indicator = Indicator::None;
    float progress = 0.0F;
    bool closable = false;

    // How long the message stays in seconds, zero to stay until something lets it go, or nothing for the duration of its style.
    std::optional<float> duration;

    MessageStack::Position position = MessageStack::Position::Bottom;
    MessageStack::Arrangement arrangement = MessageStack::Arrangement::List;
    bool expandable = true;
};

} // namespace haylen::ui
