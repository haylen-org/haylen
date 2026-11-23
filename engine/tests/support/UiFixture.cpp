#include "support/UiFixture.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/GamepadState.hpp"
#include "haylen/ui/Backend.hpp"

namespace haylen::test {

UiFixture::UiFixture(std::map<std::string, std::string> files) : UiFixture(std::make_unique<EngineFixture>(std::move(files))) {}

UiFixture::UiFixture(std::unique_ptr<EngineFixture> engineFixture) : fixture(std::move(engineFixture)) {
    // clang-format off
    connection = getUi().events.connect([this](ui::Gui&, const ui::Event& event) {
        events.push_back(event);
    });
    // clang-format on
}

plugins::UiPlugin& UiFixture::getUi() {
    return getEngine().getPlugin<plugins::UiPlugin>();
}

std::shared_ptr<ui::Gui> UiFixture::mount(const std::string& json, ui::Placement placement, int layer) {
    std::shared_ptr<ui::Gui> gui = getUi().createGui(core::Json::parse(json), placement);
    getUi().mount(gui, layer);
    frames();
    return gui;
}

void UiFixture::frames(int count) {
    fixture->frames(count);
}

float UiFixture::getMetric(ui::Theme::Metric role) {
    return getUi().getTheme().getMetric(role);
}

int UiFixture::getTransitionFrames() {
    return static_cast<int>(getMetric(ui::Theme::Metric::TransitionDuration) * 60.0F) + 2;
}

std::string UiFixture::labels(int count) {
    std::string list;
    for (int index = 1; index <= count; ++index) {
        list += (index > 1 ? ", " : "") + std::string(R"({"kind": "label", "text": "Line )") + std::to_string(index) + R"("})";
    }

    return list;
}

bool UiFixture::hasClip(const math::Rect& area, float tolerance) {
    const ImDrawData* data = ImGui::GetDrawData();
    if (data == nullptr) {
        throw std::logic_error("The UI drew no frame.");
    }

    for (const ImDrawList* list : data->CmdLists) {
        for (const ImDrawCmd& command : list->CmdBuffer) {
            const math::Rect clip = math::Rect::fromMinMax({command.ClipRect.x, command.ClipRect.y}, {command.ClipRect.z, command.ClipRect.w});
            const bool near = std::fabs(clip.x - area.x) <= tolerance && std::fabs(clip.y - area.y) <= tolerance && std::fabs(clip.getRight() - area.getRight()) <= tolerance && std::fabs(clip.getBottom() - area.getBottom()) <= tolerance;
            if (command.UserCallback != nullptr && near) {
                return true;
            }
        }
    }

    return false;
}

void UiFixture::pointer(platform::Event::Type type, math::Vec2 point, input::MouseButton pressed) {
    platform::Event event;
    event.type = type;
    event.mouseButton = pressed;
    event.position = getEngine().getViewport().toFramebuffer(point + getEngine().getViewport().getVisibleRect().getMin());
    getEngine().handleEvent(event);
}

void UiFixture::touch(platform::Event::Type type, std::uint64_t id, math::Vec2 point) {
    platform::Event event;
    event.type = type;
    event.touchCount = 1;
    event.touches[0] = {.id = id, .position = getEngine().getViewport().toFramebuffer(point + getEngine().getViewport().getVisibleRect().getMin()), .changed = true};
    getEngine().handleEvent(event);
}

void UiFixture::click(math::Vec2 point) {
    pointer(platform::Event::Type::MouseMove, point);
    frames();
    pointer(platform::Event::Type::MouseDown, point);
    frames();
    pointer(platform::Event::Type::MouseUp, point);
    frames(2);
}

void UiFixture::click(const ui::Gui& gui, std::string_view id) {
    click(getBounds(gui, id).getCenter());
}

void UiFixture::drag(math::Vec2 from, math::Vec2 to, int steps) {
    pointer(platform::Event::Type::MouseMove, from);
    frames();
    pointer(platform::Event::Type::MouseDown, from);
    frames();

    for (int step = 1; step <= steps; ++step) {
        pointer(platform::Event::Type::MouseMove, from + (to - from) * (static_cast<float>(step) / static_cast<float>(steps)));
        frames();
    }

    pointer(platform::Event::Type::MouseUp, to);
    frames(2);
}

void UiFixture::swipe(math::Vec2 from, math::Vec2 to, int steps) {
    touch(platform::Event::Type::TouchBegan, 1, from);
    frames();

    for (int step = 1; step <= steps; ++step) {
        touch(platform::Event::Type::TouchMoved, 1, from + (to - from) * (static_cast<float>(step) / static_cast<float>(steps)));
        frames();
    }

    frames(20);
    touch(platform::Event::Type::TouchEnded, 1, to);
    frames(kSettleFrames);
}

void UiFixture::wheel(math::Vec2 point, float delta, float horizontal, bool precise) {
    pointer(platform::Event::Type::MouseMove, point);
    platform::Event event;
    event.type = platform::Event::Type::MouseScroll;
    event.scroll = {horizontal, delta};
    event.preciseScroll = precise;
    getEngine().handleEvent(event);
    frames();
}

void UiFixture::key(input::Key code) {
    for (const platform::Event::Type type : {platform::Event::Type::KeyDown, platform::Event::Type::KeyUp}) {
        platform::Event event;
        event.type = type;
        event.key = code;
        getEngine().handleEvent(event);
        frames();
    }
}

void UiFixture::button(input::GamepadButton pressed) {
    input::GamepadState state{.connected = true};
    state.buttons[static_cast<std::size_t>(pressed)] = true;
    fixture->host().setGamepad(0, state);
    frames();
    fixture->host().setGamepad(0, input::GamepadState{.connected = true});
    frames();
}

void UiFixture::stick(math::Vec2 value) {
    input::GamepadState state{.connected = true};
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftX)] = value.x;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftY)] = value.y;
    fixture->host().setGamepad(0, state);
    frames();
}

void UiFixture::type(std::u32string_view text) {
    for (const char32_t character : text) {
        platform::Event event;
        event.type = platform::Event::Type::Character;
        event.character = character;
        getEngine().handleEvent(event);
        frames();
    }
}

ui::Component& UiFixture::getComponent(const ui::Gui& gui, std::string_view id) {
    ui::Component* component = gui.find(id);
    if (component == nullptr) {
        throw std::logic_error("The GUI has no component \"" + std::string(id) + "\".");
    }

    return *component;
}

const math::Rect& UiFixture::getBounds(const ui::Gui& gui, std::string_view id) const {
    return getComponent(gui, id).getBounds();
}

const ImDrawData& UiFixture::getDrawData(plugins::UiPlugin& ui) {
    ui.getBackend().makeCurrent();
    const ImDrawData* data = ImGui::GetDrawData();
    if (data == nullptr) {
        throw std::logic_error("The UI drew no frame.");
    }

    return *data;
}

const ImDrawList& UiFixture::getGuiDrawList(plugins::UiPlugin& ui) {
    ui.getBackend().makeCurrent();
    const ImGuiWindow* window = ImGui::FindWindowByName("##haylen-guis");
    if (window == nullptr || window->DrawList == nullptr) {
        throw std::logic_error("The UI has no window of the GUIs.");
    }

    return *window->DrawList;
}

bool UiFixture::isFocused(const ui::Gui& gui, std::string_view id) {
    const ui::FocusNavigator& focus = getUi().getFocus();
    return focus.getFocusedGui() == &gui && focus.getFocusedName() == id;
}

std::string UiFixture::getFocusedItem() {
    return std::string(getUi().getFocus().getFocusedItem());
}

const ui::Event& UiFixture::getLastEvent() const {
    if (events.empty()) {
        throw std::logic_error("No event was reported.");
    }

    return events.back();
}

const ui::Event& UiFixture::findLastEvent(std::string_view name) const {
    const auto found = std::ranges::find(events.rbegin(), events.rend(), name, &ui::Event::name);
    if (found == events.rend()) {
        throw std::logic_error("No \"" + std::string(name) + "\" event was reported.");
    }

    return *found;
}

std::vector<std::string> UiFixture::getEventNames() const {
    std::vector<std::string> names;
    for (const ui::Event& event : events) {
        names.push_back(event.id + ":" + event.name);
    }

    return names;
}

} // namespace haylen::test
