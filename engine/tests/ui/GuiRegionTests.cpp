#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "haylen/core/Engine.hpp"
#include "haylen/core/SceneManager.hpp"
#include "support/DrawingScene.hpp"
#include "support/RecordingEffect.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

// The frame of an app: a bar of 80 units on top and the middle under it, where the pages of the app lay out.
class GuiRegionTest : public ::testing::Test, public test::UiFixture {
  protected:
    GuiRegionTest() {
        frame = mount(R"({"kind": "column", "gap": 0, "children": [{"kind": "row", "id": "bar", "height": 80, "children": [{"kind": "button", "id": "home", "text": "Home"}]}, {"kind": "column", "id": "content", "grow": 1}]})", Placement::Screen, 1);
    }

    std::shared_ptr<Gui> mountPage() {
        std::shared_ptr<Gui> page = getUi().createGui(core::Json::parse(R"({"kind": "column", "children": [{"kind": "button", "id": "open", "text": "Open", "align": "stretch"}]})"), Placement::Region);
        getUi().mount(page, 0, nullptr, plugins::UiPlugin::Region{.gui = frame, .node = "content"});
        frames();
        return page;
    }

    std::shared_ptr<Gui> frame;
};

} // namespace

TEST_F(GuiRegionTest, LaysOutInTheAreaOfTheNodeOfItsRegion) {
    const std::shared_ptr<Gui> page = mountPage();
    const math::Rect& content = getBounds(*frame, "content");
    EXPECT_FLOAT_EQ(getBounds(*page, "open").y, content.y);
    EXPECT_FLOAT_EQ(getBounds(*page, "open").width, content.width);

    clearEvents();
    click(*page, "open");
    EXPECT_EQ(findLastEvent("click").id, "open");
}

// A page draws right after its frame, whatever the layers, so it follows a bar that changes in the frame the bar changes.
TEST_F(GuiRegionTest, FollowsTheNodeOfItsRegionInTheFrameItMoves) {
    const std::shared_ptr<Gui> page = mountPage();
    frame->set("bar", core::Json::parse(R"({"height": 200})"));
    frames();
    EXPECT_FLOAT_EQ(getBounds(*page, "open").y, getBounds(*frame, "content").y);
    EXPECT_GE(getBounds(*page, "open").y, 200.0F);
}

TEST_F(GuiRegionTest, DrawsNothingWhileTheGuiOfItsRegionIsHidden) {
    const std::shared_ptr<Gui> page = mountPage();
    const math::Rect open = getBounds(*page, "open");
    frame->setVisible(false);
    frames(2);
    clearEvents();
    click(open.getCenter());
    EXPECT_TRUE(getEventNames().empty());

    frame->setVisible(true);
    frames(2);
    click(*page, "open");
    EXPECT_EQ(findLastEvent("click").id, "open");
}

// A page of a scene that leaves through a transition goes into the image of its scene in the area the frame gives it, while the frame stays on the screen.
TEST_F(GuiRegionTest, LeavesWithItsSceneInTheAreaOfItsRegion) {
    const auto menu = std::make_shared<test::DrawingScene>([](core::Engine&) {});
    getEngine().getScenes().push(menu);
    frames();
    std::shared_ptr<Gui> page = getUi().createGui(core::Json::parse(R"({"kind": "button", "id": "open", "text": "Open"})"), Placement::Region);
    getUi().mount(page, 0, menu, plugins::UiPlugin::Region{.gui = frame, .node = "content"});
    frames(2);
    const float top = getBounds(*page, "open").y;
    EXPECT_GE(top, getBounds(*frame, "content").y);

    getEngine().getScenes().push(std::make_shared<test::DrawingScene>([](core::Engine&) {}), {.transition = {.duration = 1.0F, .effect = std::make_shared<test::RecordingEffect>(0.0F, 1.0F)}});
    frames(10);
    ASSERT_TRUE(getEngine().getScenes().isTransitioning());
    EXPECT_FLOAT_EQ(getBounds(*page, "open").y, top);
}

TEST_F(GuiRegionTest, RefusesARegionItCannotLayOutIn) {
    const std::shared_ptr<Gui> page = getUi().createGui(core::Json::parse(R"({"kind": "label", "text": "Page"})"), Placement::Region);
    const std::shared_ptr<Gui> loose = getUi().createGui(core::Json::parse(R"({"kind": "column", "id": "content"})"), Placement::Screen);
    EXPECT_THROW(getUi().mount(page), std::invalid_argument);
    EXPECT_THROW(getUi().mount(page, 0, nullptr, plugins::UiPlugin::Region{.gui = loose, .node = "content"}), std::invalid_argument);
    EXPECT_THROW(getUi().mount(page, 0, nullptr, plugins::UiPlugin::Region{.gui = frame, .node = "sidebar"}), std::invalid_argument);
    EXPECT_THROW(getUi().mount(loose, 0, nullptr, plugins::UiPlugin::Region{.gui = frame, .node = "content"}), std::invalid_argument);

    getUi().mount(page, 0, nullptr, plugins::UiPlugin::Region{.gui = frame, .node = "content"});
    const std::shared_ptr<Gui> nested = getUi().createGui(core::Json::parse(R"({"kind": "label", "text": "Nested"})"), Placement::Region);
    EXPECT_THROW(getUi().mount(nested, 0, nullptr, plugins::UiPlugin::Region{.gui = page, .node = "content"}), std::invalid_argument);
}

TEST_F(GuiRegionTest, MountsIntoARegionFromLua) {
    // clang-format off
    getFixture().runLua(R"(
        local ui = require('haylen.ui')
        frame = ui.mount(ui.column{gap = 0, ui.row{height = 80, ui.label{text = 'Bar'}}, ui.column{id = 'content', grow = 1}}, {placement = 'screen', layer = 1})
        page = ui.mount(ui.button{id = 'open', text = 'Open'}, {placement = {gui = frame, id = 'content'}})
    )");
    // clang-format on
    frames(2);
    EXPECT_EQ(getFixture().lua("return page.placement"), "region");
    EXPECT_EQ(getFixture().lua("return tostring(page:bounds('open').y >= frame:bounds('content').y)"), "true");
    EXPECT_NE(getFixture().lua("ui = require('haylen.ui') ui.mount(ui.label{}, {placement = 'region'})").find("\"safe\", \"screen\" or a region"), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.mount(ui.label{}, {placement = {gui = frame, node = 'content'}})").find("Unknown option \"node\""), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.mount(ui.label{}, {placement = {id = 'content'}})").find("names its GUI with \"gui\""), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.mount(ui.label{}, {placement = {gui = frame, id = 'menu'}})").find("no node with the id \"menu\""), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.mount(ui.label{}, {placement = {gui = frame, id = 'content'}, layer = 2})").find("\"layer\""), std::string::npos);
}

} // namespace haylen::ui
