#include "ui/components/inputs/KeyCapture.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/input/ActionMap.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

std::string KeyCapture::describe(std::string_view binding) {
    const std::size_t separator = binding.find(':');
    const std::string_view kind = binding.substr(0, separator);
    std::string name(separator == std::string_view::npos ? std::string_view{} : binding.substr(separator + 1));
    std::string sign;
    if (!name.empty() && (name.back() == '+' || name.back() == '-')) {
        sign = std::string(" ") + name.back();
        name.pop_back();
    }

    // Words split at underscores and start with a capital letter, so left_shift reads Left Shift.
    bool wordStart = true;
    for (char& letter : name) {
        if (letter == '_') {
            letter = ' ';
            wordStart = true;
        } else if (wordStart) {
            letter = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
            wordStart = false;
        }
    }
    if (kind == "mouse") {
        return "Mouse " + name;
    }
    if (kind == "button") {
        return "Button " + name;
    }
    if (kind == "stick") {
        return name + " Stick";
    }
    return name + sign;
}

std::vector<std::string> KeyCapture::readBindings(PropertyReader& reader, std::string_view key, const core::Json& listed) {
    if (!PropertyReader::isList(listed)) {
        reader.fail(key, "must be a list of bindings");
    }
    std::vector<std::string> bindings;
    for (const core::Json& entry : listed) {
        if (!entry.is_string() || !input::ActionMap::Binding::parse(entry.get<std::string>())) {
            reader.fail(key, "must be a list of bindings such as key:escape");
        }
        bindings.push_back(entry.get<std::string>());
    }
    return bindings;
}

void KeyCapture::readProperties(PropertyReader& reader) {
    reader.read("value", value);
    if (!value.empty() && !input::ActionMap::Binding::parse(value)) {
        reader.fail("value", "must be a binding such as key:space");
    }
    reader.read("placeholder", placeholder);
    reader.read("prompt", prompt);
    if (const core::Json* listed = reader.take("sources")) {
        if (!PropertyReader::isList(*listed) || !std::ranges::all_of(*listed, [](const core::Json& entry) { return entry.is_string() && std::ranges::find(kSourceNames, entry.get<std::string>()) != kSourceNames.end(); })) {
            reader.fail("sources", "must list key, mouse, button or axis");
        }
        sources.clear();
        for (const core::Json& entry : *listed) {
            sources.push_back(entry.get<std::string>());
        }
    }
    if (const core::Json* listed = reader.take("cancelWith")) {
        cancelWith = readBindings(reader, "cancelWith", *listed);
    }
}

math::Vec2 KeyCapture::measureContent(Context& context, float availableWidth) {
    return {std::min(availableWidth, 320.0F), context.getMetric(Theme::Metric::ControlHeight)};
}

void KeyCapture::render(Context& context, const math::Rect& bounds) {
    const float radius = context.getMetric(Theme::Metric::ControlRadius);
    const Widgets::Interaction state = Widgets::interact(context, bounds, radius, "##capture");
    const ImGuiID id = ImGui::GetItemID();
    if (takeFocusRequest()) {
        Widgets::focusItem(context);
    }
    if (state.clicked && !capturing) {
        start(context.getInput(), context.getFrame());
    }

    // While it listens, the field holds every key, button and the pointer, so nothing else in the UI reacts to the input it takes.
    if (capturing) {
        ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
        ImGui::SetActiveIdUsingAllKeyboardKeys();
        for (const ImGuiKey key : {ImGuiKey_GamepadFaceDown, ImGuiKey_GamepadFaceRight, ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadDpadDown, ImGuiKey_MouseLeft}) {
            ImGui::SetKeyOwner(key, id);
        }
        if (context.getFrame() > startedFrame) {
            if (const std::optional<std::string> binding = listen(context.getInput())) {
                finish(context, *binding);
            }
        }
    }

    Surfaces::draw(context, capturing ? Theme::Surface::FieldFocused : Theme::Surface::Field, bounds, context.getColor(state.hovered && !capturing ? Theme::Color::BorderStrong : Theme::Color::Raised), context.getColor(capturing ? Theme::Color::Focus : Theme::Color::Border));
    const math::Rect inner = bounds.inset(Surfaces::getPadding(context, Theme::Surface::Field));
    std::string shown = capturing ? context.getText(prompt) : (value.empty() ? context.getText(placeholder) : describe(value));
    if (capturing && shown.empty()) {
        shown = "\xE2\x80\xA6";
    }
    const bool muted = capturing || value.empty();
    Typography::drawAligned(context, Theme::Font::Body, inner, context.getColor(muted ? Theme::Color::TextMuted : Theme::Color::Text), shown, Alignment::Center);
}

void KeyCapture::drawingStopped(Context&) {
    capturing = false;
}

bool KeyCapture::accepts(Source source) const {
    return sources.empty() || std::ranges::find(sources, kSourceNames[static_cast<std::size_t>(source)]) != sources.end();
}

void KeyCapture::start(const input::Input& devices, std::uint64_t frame) {
    capturing = true;
    startedFrame = frame;
    for (std::size_t pad = 0; pad < input::Input::kMaxGamepads; ++pad) {
        resting[pad] = devices.getGamepad(pad).axes;
    }
}

// Returns the first binding the devices pressed this frame, with sticks and triggers counting once they travel well away from where they rested when listening started.
std::optional<std::string> KeyCapture::listen(const input::Input& devices) const {
    for (std::size_t code = 0; code < input::Controls::kKeyCount; ++code) {
        const auto key = static_cast<input::Key>(code);
        if (devices.isKeyPressed(key) && input::Controls::keyName(key) != "unknown") {
            return "key:" + std::string(input::Controls::keyName(key));
        }
    }
    for (const input::MouseButton button : {input::MouseButton::Left, input::MouseButton::Right, input::MouseButton::Middle}) {
        if (devices.isMousePressed(button)) {
            return "mouse:" + std::string(input::Controls::mouseButtonName(button));
        }
    }
    for (std::size_t pad = 0; pad < input::Input::kMaxGamepads; ++pad) {
        for (std::size_t index = 0; index < input::Controls::kGamepadButtonCount; ++index) {
            const auto button = static_cast<input::GamepadButton>(index);
            if (devices.isGamepadPressed(pad, button)) {
                return "button:" + std::string(input::Controls::gamepadButtonName(button));
            }
        }
        for (std::size_t index = 0; index < input::Controls::kGamepadAxisCount; ++index) {
            const float travel = devices.getGamepad(pad).axes[index] - resting[pad][index];
            if (std::abs(travel) > kAxisTravel) {
                return "axis:" + std::string(input::Controls::gamepadAxisName(static_cast<input::GamepadAxis>(index))) + (travel > 0.0F ? "+" : "-");
            }
        }
    }
    return std::nullopt;
}

// A binding of cancelWith stops listening, a binding of a kind the field does not take is ignored, and any other becomes the value.
void KeyCapture::finish(Context& context, const std::string& binding) {
    if (std::ranges::find(cancelWith, binding) != cancelWith.end()) {
        capturing = false;
        ImGui::ClearActiveID();
        context.emit(*this, "cancel");
        return;
    }
    const std::string_view kind = std::string_view(binding).substr(0, binding.find(':'));
    const auto source = static_cast<Source>(std::ranges::find(kSourceNames, kind) - kSourceNames.begin());
    if (!accepts(source)) {
        return;
    }
    capturing = false;
    ImGui::ClearActiveID();
    value = binding;
    context.emit(*this, "change", {{"value", value}});
}

} // namespace haylen::ui
