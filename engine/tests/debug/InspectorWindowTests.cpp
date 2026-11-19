#include <gtest/gtest.h>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Backend.hpp"
#include "support/UiFixture.hpp"

namespace haylen::debug {

class InspectorWindowTest : public ::testing::Test, public test::UiFixture {
  protected:
    InspectorWindowTest() : test::UiFixture({{"haylen.json", R"({"name": "Inspector", "identifier": "com.haylen.inspector", "debug": {"console": true}})"}}) {}

    // Turns a point of ImGui into the point of the visible area that the touches of the fixture take.
    [[nodiscard]] math::Vec2 place(ImVec2 point) {
        return getUi().getBackend().toDesign({point.x, point.y}) - getEngine().getViewport().getVisibleRect().getMin();
    }

    void swipe(math::Vec2 from, math::Vec2 to) {
        touch(platform::Event::Type::TouchBegan, 1, from);
        frames();

        for (int step = 1; step <= 8; ++step) {
            touch(platform::Event::Type::TouchMoved, 1, from + (to - from) * (static_cast<float>(step) / 8.0F));
            frames();
        }

        touch(platform::Event::Type::TouchEnded, 1, to);
        frames(2);
    }

    void tap(math::Vec2 point) {
        touch(platform::Event::Type::TouchBegan, 1, point);
        frames();
        touch(platform::Event::Type::TouchEnded, 1, point);
        frames(2);
    }
};

// A finger moves the window by its title bar, opens rows with a tap and scrolls the rows when it drags them, and the window stays inside the safe area.
TEST_F(InspectorWindowTest, FollowsAFingerAndStaysInsideTheSafeArea) {
    getFixture().runLua("values = {} for index = 1, 80 do values['value' .. index] = index end require('haylen.debug').watch('values', values) require('haylen.scene').push({}) require('haylen.debug').setInspectorVisible(true)");
    frames(2);
    ui::Backend& backend = getUi().getBackend();
    backend.makeCurrent();
    ImGuiWindow* window = ImGui::FindWindowByName("Inspector");
    ASSERT_NE(window, nullptr);
    const math::Rect safe = backend.getSafeRect();
    EXPECT_LE(window->Pos.x + window->Size.x, safe.x + safe.width);
    EXPECT_GE(window->Pos.y, safe.y);

    // The finger grabs the title bar near its right end and carries the window farther than the width of the window.
    const ImVec2 start = window->Pos;
    const math::Vec2 title = place({window->Pos.x + window->Size.x - 80.0F, window->Pos.y + window->TitleBarHeight * 0.5F});
    const math::Vec2 shift = place({start.x - window->Size.x - 20.0F, start.y + 10.0F}) - place(start);
    swipe(title, title + shift);
    EXPECT_NEAR(window->Pos.x, start.x - window->Size.x - 20.0F, 1.0F);
    EXPECT_NEAR(window->Pos.y, start.y + 10.0F, 1.0F);

    // A finger that drags the empty content under the rows leaves the window where it is.
    const ImVec2 kept = window->Pos;
    const math::Vec2 empty = place({window->Pos.x + 80.0F, window->Pos.y + window->Size.y - 40.0F});
    swipe(empty, empty + math::Vec2{-60.0F, -80.0F});
    EXPECT_EQ(window->Pos.x, kept.x);
    EXPECT_EQ(window->Pos.y, kept.y);

    // The first row is the root of the watched values, and the row under it the watched table. The table starts with five rows of one height, the four roots and the scene that the open root of the scenes lists.
    const ImGuiTable* table = ImGui::TableFindByID(window->GetID("##values"));
    ASSERT_NE(table, nullptr);
    const float row = (table->OuterRect.Max.y - table->OuterRect.Min.y) / 5.0F;
    const ImVec2 first{table->OuterRect.Min.x + 60.0F, table->OuterRect.Min.y + row * 0.5F};
    tap(place(first));
    tap(place({first.x, first.y + row}));
    ASSERT_GT(window->ScrollMax.y, 0.0F);

    const ImVec2 moved = window->Pos;
    const math::Vec2 rows = place({first.x, first.y + row * 6.0F});
    swipe(rows, place({first.x, first.y + row * 6.0F - 150.0F}));
    EXPECT_GT(window->Scroll.y, 100.0F);
    EXPECT_EQ(window->Pos.x, moved.x);
    EXPECT_EQ(window->Pos.y, moved.y);

    const math::Vec2 dragged = place({window->Pos.x + 80.0F, window->Pos.y + window->TitleBarHeight * 0.5F});
    swipe(dragged, dragged + math::Vec2{4000.0F, 4000.0F});
    EXPECT_LE(window->Pos.x + window->Size.x, safe.x + safe.width + 0.5F);
    EXPECT_LE(window->Pos.y + window->Size.y, safe.y + safe.height + 0.5F);
}

} // namespace haylen::debug
