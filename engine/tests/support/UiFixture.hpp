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
#include "haylen/ui/Event.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Theme.hpp"
#include "support/EngineFixture.hpp"

struct ImDrawData;
struct ImDrawList;

namespace haylen::test {

// A running engine with GUIs mounted from JSON, the input a player would give them and the events they report.
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

    std::shared_ptr<ui::Gui> mount(const std::string& json, ui::Placement placement = ui::Placement::Screen, int layer = 0);
    void frames(int count = 1);

    void pointer(platform::Event::Type type, math::Vec2 point, input::MouseButton pressed = input::MouseButton::Left);
    void touch(platform::Event::Type type, std::uint64_t id, math::Vec2 point);
    void click(math::Vec2 point);
    void click(const ui::Gui& gui, std::string_view id);
    void drag(math::Vec2 from, math::Vec2 to, int steps = 4);

    // Turns the mouse wheel over a point by steps, or moves a trackpad by points when `precise` is set, with positive deltas scrolling up, and runs a frame.
    void wheel(math::Vec2 point, float delta, float horizontal = 0.0F, bool precise = false);

    // Presses and releases a key or a gamepad button, running a frame after each.
    void key(input::Key code);
    void button(input::GamepadButton pressed);
    void stick(math::Vec2 value);
    void type(std::u32string_view text);

    [[nodiscard]] float getMetric(ui::Theme::Metric role);

    // The frames that a transition of the theme takes, with two to spare.
    [[nodiscard]] int getTransitionFrames();

    // Labels of the text "Line 1" and on, as JSON nodes separated by commas.
    [[nodiscard]] static std::string labels(int count);

    // Whether a draw of the last frame that goes around the ImGui vertices, such as a shape or a text, is cut to the area, within a distance.
    [[nodiscard]] static bool hasClip(const math::Rect& area, float tolerance = 0.0F);

    // The component of a GUI with an id, which throws when the GUI holds none, so a test never reads a component that is missing.
    [[nodiscard]] static ui::Component& getComponent(const ui::Gui& gui, std::string_view id);
    [[nodiscard]] const math::Rect& getBounds(const ui::Gui& gui, std::string_view id) const;

    // What the UI drew in its last frame and the draw list of the window that the GUIs share, which throw before the UI drew, so a test never reads a frame that is missing.
    [[nodiscard]] static const ImDrawData& getDrawData(plugins::UiPlugin& ui);
    [[nodiscard]] static const ImDrawList& getGuiDrawList(plugins::UiPlugin& ui);
    [[nodiscard]] bool isFocused(const ui::Gui& gui, std::string_view id);

    // The id of the collection item that holds the focus, empty outside collections.
    [[nodiscard]] std::string getFocusedItem();
    [[nodiscard]] std::vector<std::string> getEventNames() const;
    [[nodiscard]] const ui::Event& getLastEvent() const;
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
