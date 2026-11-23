#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/Key.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Event.hpp"
#include "haylen/ui/Gui.hpp"
#include "support/EngineFixture.hpp"
#include "support/UiFixture.hpp"

namespace haylen::test {

// A running engine whose tests of the components mount GUIs, give them input and read the events of the components, without the focus events that clicks also cause.
class ComponentFixture {
  protected:
    explicit ComponentFixture(std::map<std::string, std::string> files = {}) : ComponentFixture(std::make_unique<EngineFixture>(std::move(files))) {}
    virtual ~ComponentFixture() = default;

    ComponentFixture(const ComponentFixture&) = delete;
    ComponentFixture& operator=(const ComponentFixture&) = delete;

    // A fixture of the components of the engine that the fixture runs, such as one that runs Lua.
    explicit ComponentFixture(std::unique_ptr<EngineFixture> engineFixture) : fixture(std::move(engineFixture)) {
        // Clicks move the focus too, and the focus tests check its events, so these tests keep the events of the components alone.
        // clang-format off
        connection = getUi().events.connect([this](ui::Gui&, const ui::Event& event) {
            if (event.name != "focus" && event.name != "blur") {
                events.push_back(event);
            }
        });
        // clang-format on
    }

    core::Engine& getEngine() {
        return fixture->engine();
    }
    plugins::UiPlugin& getUi() {
        return getEngine().getPlugin<plugins::UiPlugin>();
    }

    std::shared_ptr<ui::Gui> mount(const std::string& json) {
        auto gui = getUi().createGui(core::Json::parse(json), ui::Placement::Screen);
        getUi().mount(gui);
        frames();
        return gui;
    }

    void frames(int count = 1) {
        fixture->frames(count);
    }

    void pointer(platform::Event::Type type, math::Vec2 point) {
        platform::Event event;
        event.type = type;
        event.position = getEngine().getViewport().toFramebuffer(point + getEngine().getViewport().getVisibleRect().getMin());
        getEngine().handleEvent(event);
    }

    void key(input::Key code) {
        for (const platform::Event::Type type : {platform::Event::Type::KeyDown, platform::Event::Type::KeyUp}) {
            platform::Event event;
            event.type = type;
            event.key = code;
            getEngine().handleEvent(event);
            frames();
        }
    }

    void type(std::u32string_view text) {
        for (const char32_t character : text) {
            platform::Event event;
            event.type = platform::Event::Type::Character;
            event.character = character;
            getEngine().handleEvent(event);
            frames();
        }
    }

    void touch(platform::Event::Type type, std::uint64_t id, math::Vec2 point) {
        platform::Event event;
        event.type = type;
        event.touchCount = 1;
        event.touches[0] = {.id = id, .position = getEngine().getViewport().toFramebuffer(point + getEngine().getViewport().getVisibleRect().getMin()), .changed = true};
        getEngine().handleEvent(event);
    }

    // Presses and releases at a point, then runs the frame that delivers the resulting events.
    void click(math::Vec2 point) {
        pointer(platform::Event::Type::MouseMove, point);
        frames();
        pointer(platform::Event::Type::MouseDown, point);
        frames();
        pointer(platform::Event::Type::MouseUp, point);
        frames(2);
    }

    void click(const ui::Gui& gui, const std::string& id) {
        click(getBounds(gui, id).getCenter());
    }

    [[nodiscard]] const math::Rect& getBounds(const ui::Gui& gui, const std::string& id) const {
        return UiFixture::getComponent(gui, id).getBounds();
    }

    [[nodiscard]] std::vector<std::string> getEventNames() const {
        std::vector<std::string> list;
        for (const ui::Event& event : events) {
            list.push_back(event.id + ":" + event.name);
        }

        return list;
    }

    [[nodiscard]] const ui::Event& getLastEvent() const {
        if (events.empty()) {
            throw std::runtime_error("No event was reported.");
        }

        return events.back();
    }

    // Runs a frame and counts the vertices the GUI window drew, where every glyph adds four.
    // Counts what the GUIs draw: the ImGui vertices of their window and the glyphs their text draws through the renderer.
    [[nodiscard]] int countDrawn() {
        frames();
        return UiFixture::getGuiDrawList(getUi()).VtxBuffer.Size + static_cast<int>(getEngine().getRenderer2D().getStats().instances);
    }

    std::unique_ptr<test::EngineFixture> fixture;
    std::vector<ui::Event> events;
    core::Connection connection;
};

} // namespace haylen::test
