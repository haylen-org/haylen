#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/Gui.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

using Notice = Component::Notice;

class NodeEventTest : public ::testing::Test, public test::UiFixture {
  protected:
    // Mounts a GUI whose listed nodes report the given events from the moment it mounts.
    std::shared_ptr<Gui> mountListening(const std::string& json, const std::vector<std::pair<std::string, std::vector<Notice>>>& listening) {
        std::shared_ptr<Gui> gui = getUi().createGui(core::Json::parse(json), Placement::Screen);
        for (const auto& [id, notices] : listening) {
            for (const Notice notice : notices) {
                gui->listen(id, notice, true);
            }
        }
        getUi().mount(gui, 0);
        frames(2);
        return gui;
    }

    [[nodiscard]] long countEvents(std::string_view entry) const {
        const std::vector<std::string> names = getEventNames();
        return std::ranges::count(names, entry);
    }

    // The interface reads the pointer in whole units, so a reported position is the design point of the whole unit under it.
    [[nodiscard]] math::Vec2 toDesign(math::Vec2 point) {
        return math::Vec2{std::floor(point.x), std::floor(point.y)} + getEngine().getViewport().getVisibleRect().getMin();
    }

    void scroll(math::Vec2 point, math::Vec2 steps) {
        pointer(platform::Event::Type::MouseMove, point);
        frames();
        platform::Event event;
        event.type = platform::Event::Type::MouseScroll;
        event.scroll = steps;
        getEngine().handleEvent(event);
        frames(2);
    }
};

} // namespace

TEST_F(NodeEventTest, ReportsWhenNodesJoinShowHideAndLeave) {
    const std::vector<Notice> lifecycle{Notice::Mount, Notice::Unmount, Notice::Show, Notice::Hide};
    auto gui = mountListening(R"({"kind": "column", "id": "box", "children": [{"kind": "card", "id": "card"}, {"kind": "label", "id": "plain", "text": "Plain"}]})", {{"card", lifecycle}});
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"card:mount", "card:show"}));

    clearEvents();
    gui->set("card", {{"visible", false}});
    frames(2);
    gui->set("card", {{"visible", true}});
    frames(2);
    gui->setVisible(false);
    frames(2);
    gui->setVisible(true);
    frames(2);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"card:hide", "card:show", "card:hide", "card:show"}));

    // A node that leaves reports it before the change returns, and the nodes that arrive report joining once the GUI draws.
    clearEvents();
    gui->replaceChildren("box", core::Json::parse(R"([{"kind": "card", "id": "fresh"}])"));
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"card:unmount"}));
    gui->listen("fresh", Notice::Mount, true);
    gui->listen("fresh", Notice::Unmount, true);
    frames(2);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"card:unmount", "fresh:mount"}));

    // A node that leaves before its GUI drew never joined, so it reports neither.
    clearEvents();
    gui->replaceChildren("box", core::Json::parse(R"([{"kind": "card", "id": "brief"}])"));
    gui->listen("brief", Notice::Mount, true);
    gui->listen("brief", Notice::Unmount, true);
    gui->replaceChildren("box", core::Json::parse(R"([{"kind": "card", "id": "last"}])"));
    gui->listen("last", Notice::Unmount, true);
    frames(2);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"fresh:unmount"}));

    clearEvents();
    EXPECT_TRUE(getUi().unmount(*gui));
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"last:unmount"}));

    // A GUI mounted again reports joining again.
    clearEvents();
    gui->listen("last", Notice::Mount, true);
    getUi().mount(gui, 0);
    frames(2);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"last:mount"}));
}

TEST_F(NodeEventTest, ReportsThePointerOverANode) {
    const std::vector<Notice> pointing{Notice::Hover, Notice::Press, Notice::Drag, Notice::Release, Notice::Scroll};
    auto gui = mountListening(R"({"kind": "column", "children": [{"kind": "card", "id": "card", "width": 300, "height": 200, "children": [{"kind": "button", "id": "inner", "text": "Inner"}]}, {"kind": "label", "id": "away", "text": "Away"}]})", {{"card", pointing}, {"inner", {Notice::Press}}});
    const math::Rect card = getBounds(*gui, "card");
    const math::Vec2 inside = getBounds(*gui, "inner").getCenter();
    const math::Vec2 outside = getBounds(*gui, "away").getCenter();

    pointer(platform::Event::Type::MouseMove, inside);
    frames(2);
    EXPECT_EQ(findLastEvent("hover").value, (core::Json{{"hovered", true}}));

    // Every listening node under the pointer reports the press.
    clearEvents();
    pointer(platform::Event::Type::MouseDown, inside);
    frames(2);
    EXPECT_EQ(countEvents("card:press"), 1);
    EXPECT_EQ(countEvents("inner:press"), 1);
    EXPECT_EQ(findLastEvent("press").value, (core::Json{{"x", toDesign(inside).x}, {"y", toDesign(inside).y}, {"button", "left"}}));

    // The press follows the pointer out of the node until it lets go.
    pointer(platform::Event::Type::MouseMove, inside + math::Vec2{30.0F, 0.0F});
    frames(2);
    EXPECT_EQ(findLastEvent("drag").value, (core::Json{{"x", toDesign(inside).x + 30.0F}, {"y", toDesign(inside).y}, {"deltaX", 30.0F}, {"deltaY", 0.0F}, {"button", "left"}}));
    pointer(platform::Event::Type::MouseMove, outside);
    frames(2);
    EXPECT_EQ(findLastEvent("hover").value, (core::Json{{"hovered", false}}));
    pointer(platform::Event::Type::MouseUp, outside);
    frames(2);
    EXPECT_EQ(findLastEvent("release").value, (core::Json{{"x", toDesign(outside).x}, {"y", toDesign(outside).y}, {"button", "left"}, {"inside", false}}));

    clearEvents();
    pointer(platform::Event::Type::MouseMove, card.getCenter());
    frames();
    pointer(platform::Event::Type::MouseDown, card.getCenter(), input::MouseButton::Right);
    frames(2);
    pointer(platform::Event::Type::MouseUp, card.getCenter(), input::MouseButton::Right);
    frames(2);
    EXPECT_EQ(findLastEvent("press").value.at("button"), "right");
    EXPECT_EQ(findLastEvent("release").value.at("inside"), true);
    EXPECT_EQ(countEvents("card:drag"), 0);

    scroll(card.getCenter(), {0.0F, -2.0F});
    EXPECT_EQ(findLastEvent("scroll").value, (core::Json{{"deltaX", 0.0F}, {"deltaY", -2.0F}}));

    // A disabled node and the nodes inside it report nothing of the pointer, and the button inside only loses the focus.
    clearEvents();
    gui->set("card", {{"enabled", false}});
    click(card.getCenter());
    click(inside);
    scroll(card.getCenter(), {0.0F, 1.0F});
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"inner:blur"}));
}

TEST_F(NodeEventTest, LetsGoOfThePointerWhenANodeStopsDrawing) {
    auto gui = mountListening(R"({"kind": "column", "children": [{"kind": "card", "id": "card", "width": 300, "height": 200}]})", {{"card", {Notice::Hover, Notice::Press, Notice::Release, Notice::Hide}}});
    const math::Vec2 center = getBounds(*gui, "card").getCenter();
    pointer(platform::Event::Type::MouseMove, center);
    frames();
    pointer(platform::Event::Type::MouseDown, center);
    frames(2);
    clearEvents();
    gui->set("card", {{"visible", false}});
    frames(2);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"card:hover", "card:release", "card:hide"}));
    EXPECT_EQ(findLastEvent("release").value.at("inside"), false);
    pointer(platform::Event::Type::MouseUp, center);
    frames(2);
    EXPECT_EQ(countEvents("card:release"), 1);
}

TEST_F(NodeEventTest, KeepsThePressesOfTouchButtonsToThemselves) {
    auto gui = mountListening(R"({"kind": "touchButton", "id": "jump", "action": "jump", "width": 160, "height": 160})", {{"jump", {Notice::Press, Notice::Release}}});
    click(*gui, "jump");
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"jump:press", "jump:release"}));
    EXPECT_TRUE(findLastEvent("press").value.empty());
}

TEST_F(NodeEventTest, NamesTheEventsEveryKindReports) {
    for (std::size_t index = 0; index < Component::kNoticeNames.size(); ++index) {
        EXPECT_EQ(Component::noticeFromName(Component::kNoticeNames[index]), static_cast<Notice>(index));
    }
    EXPECT_FALSE(Component::noticeFromName("click").has_value());
}

} // namespace haylen::ui
