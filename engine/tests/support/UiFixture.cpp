#include "support/UiFixture.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/GamepadState.hpp"

namespace haylen::test {

UiFixture::UiFixture(std::map<std::string, std::string> files) : fixture(std::move(files)) {
    // clang-format off
    connection = getUi().events.connect([this](ui::Document&, const ui::Event& event) {
        events.push_back(event);
    });
    // clang-format on
}

plugins::UiPlugin& UiFixture::getUi() {
    return getEngine().getPlugin<plugins::UiPlugin>();
}

std::shared_ptr<ui::Document> UiFixture::mount(const std::string& json, ui::Placement placement, int layer) {
    std::shared_ptr<ui::Document> document = getUi().createDocument(core::Json::parse(json), placement);
    getUi().mount(document, layer);
    frames();
    return document;
}

void UiFixture::frames(int count) {
    fixture.frames(count);
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

void UiFixture::click(const ui::Document& document, std::string_view id) {
    click(getBounds(document, id).getCenter());
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
    fixture.host().setGamepad(0, state);
    frames();
    fixture.host().setGamepad(0, input::GamepadState{.connected = true});
    frames();
}

void UiFixture::stick(math::Vec2 value) {
    input::GamepadState state{.connected = true};
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftX)] = value.x;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftY)] = value.y;
    fixture.host().setGamepad(0, state);
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

const math::Rect& UiFixture::getBounds(const ui::Document& document, std::string_view id) const {
    return document.find(id)->getBounds();
}

bool UiFixture::isFocused(const ui::Document& document, std::string_view id) {
    const ui::FocusNavigator& focus = getUi().getFocus();
    return focus.getFocusedDocument() == &document && focus.getFocusedName() == id;
}

const ui::Event& UiFixture::findLastEvent(std::string_view name) const {
    const auto found = std::ranges::find(events.rbegin(), events.rend(), name, &ui::Event::name);
    if (found == events.rend()) {
        throw std::logic_error("No " + std::string(name) + " event was reported.");
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
