#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Connection.hpp"
#include "haylen/input/GamepadButton.hpp"
#include "haylen/input/Key.hpp"
#include "haylen/input/MouseButton.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Document.hpp"
#include "haylen/ui/Event.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::test {

// A running engine with documents mounted from JSON, the input a player would give them and the events they report.
class UiFixture {
  public:
    explicit UiFixture(std::map<std::string, std::string> files = {});

    [[nodiscard]] core::Engine& getEngine() noexcept {
        return fixture.engine();
    }
    [[nodiscard]] EngineFixture& getFixture() noexcept {
        return fixture;
    }
    [[nodiscard]] plugins::UiPlugin& getUi();

    std::shared_ptr<ui::Document> mount(const std::string& json, ui::Placement placement = ui::Placement::Screen, int layer = 0);
    void frames(int count = 1);

    void pointer(platform::Event::Type type, math::Vec2 point, input::MouseButton pressed = input::MouseButton::Left);
    void touch(platform::Event::Type type, std::uint64_t id, math::Vec2 point);
    void click(math::Vec2 point);
    void click(const ui::Document& document, std::string_view id);
    void drag(math::Vec2 from, math::Vec2 to, int steps = 4);

    // Presses and releases a key or a gamepad button, running a frame after each.
    void key(input::Key code);
    void button(input::GamepadButton pressed);
    void stick(math::Vec2 value);
    void type(std::u32string_view text);

    [[nodiscard]] const math::Rect& getBounds(const ui::Document& document, std::string_view id) const;
    [[nodiscard]] bool isFocused(const ui::Document& document, std::string_view id);
    [[nodiscard]] std::vector<std::string> getEventNames() const;
    [[nodiscard]] const ui::Event& getLastEvent() const {
        return events.back();
    }
    [[nodiscard]] const ui::Event& findLastEvent(std::string_view name) const;
    void clearEvents() noexcept {
        events.clear();
    }

  private:
    EngineFixture fixture;
    std::vector<ui::Event> events;
    core::Connection connection;
};

} // namespace haylen::test
