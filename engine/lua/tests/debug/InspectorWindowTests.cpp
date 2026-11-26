#include <gtest/gtest.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>

#include "debug/InspectorWindow.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/platform/Accessibility.hpp"
#include "haylen/platform/AccessibilityElement.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Backend.hpp"
#include "support/AllocationTracker.hpp"
#include "support/LuaUiFixture.hpp"

namespace haylen::lua::debug {

class InspectorWindowTest : public ::testing::Test, public test::LuaUiFixture {
  protected:
    InspectorWindowTest() : test::LuaUiFixture({{"haylen.json", R"({"name": "Inspector", "identifier": "com.haylen.inspector", "debug": {"console": true}})"}}) {}

    // Turns a point of ImGui into the point of the visible area that the touches of the fixture take.
    [[nodiscard]] haylen::math::Vec2 place(ImVec2 point) {
        return getUi().getBackend().toDesign({point.x, point.y}) - getEngine().getViewport().getVisibleRect().getMin();
    }

    void swipe(haylen::math::Vec2 from, haylen::math::Vec2 to) {
        touch(haylen::platform::Event::Type::TouchBegan, 1, from);
        frames();

        for (int step = 1; step <= 8; ++step) {
            touch(haylen::platform::Event::Type::TouchMoved, 1, from + (to - from) * (static_cast<float>(step) / 8.0F));
            frames();
        }

        touch(haylen::platform::Event::Type::TouchEnded, 1, to);
        frames(2);
    }

    void tap(haylen::math::Vec2 point) {
        touch(haylen::platform::Event::Type::TouchBegan, 1, point);
        frames();
        touch(haylen::platform::Event::Type::TouchEnded, 1, point);
        frames(2);
    }

    // Returns the line that the inspector shows over its values as screen readers read it, the text that follows the dialog of the window in the tree of the UI.
    [[nodiscard]] std::string readLine() {
        const std::span<const haylen::platform::AccessibilityElement> elements = getEngine().getAccessibility().getElements();
        const auto window = std::ranges::find_if(elements, [](const haylen::platform::AccessibilityElement& element) { return element.role == haylen::platform::AccessibilityElement::Role::Dialog && element.label == "Inspector"; });
        const auto line = std::find_if(window, elements.end(), [](const haylen::platform::AccessibilityElement& element) { return element.role == haylen::platform::AccessibilityElement::Role::Text; });
        return line != elements.end() ? line->label : std::string();
    }
};

// A finger moves the window by its title bar, opens rows with a tap and scrolls the rows when it drags them, and the window stays inside the safe area.
TEST_F(InspectorWindowTest, FollowsAFingerAndStaysInsideTheSafeArea) {
    getFixture().runLua("values = {} for index = 1, 80 do values['value' .. index] = index end require('haylen.debug').watch('values', values) require('haylen.scene').push({}) require('haylen.debug').setInspectorVisible(true)");
    frames(2);
    haylen::ui::Backend& backend = getUi().getBackend();
    backend.makeCurrent();
    ImGuiWindow* window = ImGui::FindWindowByName("Inspector");
    ASSERT_NE(window, nullptr);
    const haylen::math::Rect safe = backend.getSafeRect();
    EXPECT_LE(window->Pos.x + window->Size.x, safe.x + safe.width);
    EXPECT_GE(window->Pos.y, safe.y);

    // The finger grabs the title bar near its right end and carries the window farther than the width of the window.
    const ImVec2 start = window->Pos;
    const haylen::math::Vec2 title = place({window->Pos.x + window->Size.x - 80.0F, window->Pos.y + window->TitleBarHeight * 0.5F});
    const haylen::math::Vec2 shift = place({start.x - window->Size.x - 20.0F, start.y + 10.0F}) - place(start);
    swipe(title, title + shift);
    EXPECT_NEAR(window->Pos.x, start.x - window->Size.x - 20.0F, 1.0F);
    EXPECT_NEAR(window->Pos.y, start.y + 10.0F, 1.0F);

    // A finger that drags the empty content under the rows leaves the window where it is.
    const ImVec2 kept = window->Pos;
    const haylen::math::Vec2 empty = place({window->Pos.x + 80.0F, window->Pos.y + window->Size.y - 40.0F});
    swipe(empty, empty + haylen::math::Vec2{-60.0F, -80.0F});
    EXPECT_EQ(window->Pos.x, kept.x);
    EXPECT_EQ(window->Pos.y, kept.y);

    // The first row is the root of the watched values, and the row under it the watched table. The table starts with six rows of one height, the five roots and the scene that the open root of the scenes lists.
    const ImGuiTable* table = ImGui::TableFindByID(window->GetID("##values"));
    ASSERT_NE(table, nullptr);
    const float row = (table->OuterRect.Max.y - table->OuterRect.Min.y) / 6.0F;
    const ImVec2 first{table->OuterRect.Min.x + 60.0F, table->OuterRect.Min.y + row * 0.5F};
    tap(place(first));
    tap(place({first.x, first.y + row}));
    ASSERT_GT(window->ScrollMax.y, 0.0F);

    const ImVec2 moved = window->Pos;
    const haylen::math::Vec2 rows = place({first.x, first.y + row * 6.0F});
    swipe(rows, place({first.x, first.y + row * 6.0F - 150.0F}));
    EXPECT_GT(window->Scroll.y, 100.0F);
    EXPECT_EQ(window->Pos.x, moved.x);
    EXPECT_EQ(window->Pos.y, moved.y);

    const haylen::math::Vec2 dragged = place({window->Pos.x + 80.0F, window->Pos.y + window->TitleBarHeight * 0.5F});
    swipe(dragged, dragged + haylen::math::Vec2{4000.0F, 4000.0F});
    EXPECT_LE(window->Pos.x + window->Size.x, safe.x + safe.width + 0.5F);
    EXPECT_LE(window->Pos.y + window->Size.y, safe.y + safe.height + 0.5F);
}

// The inspector lists the values again into the storage it keeps, with the path of every row, so frames that show the same rows allocate no more than frames without the window, also when every frame refreshes them.
TEST_F(InspectorWindowTest, DrawsTheSameRowsWithoutAllocating) {
    getFixture().runLua("values = {flag = true, title = 'A title that is longer than a short string'} for index = 1, 80 do values['a value whose name is longer than a short string ' .. index] = index * 0.5 end require('haylen.debug').watch('values', values) require('haylen.scene').push({})");
    // clang-format off
    const auto measure = [this] {
        frames(5);
        const test::AllocationTracker tracker;
        for (int frame = 0; frame < 10; ++frame) {
            getFixture().frames(1, haylen::debug::InspectorWindow::kRefreshSeconds);
        }
        return tracker.getTotal();
    };
    // clang-format on
    const std::size_t quiet = measure();

    getFixture().runLua("require('haylen.debug').setInspectorVisible(true)");
    frames(2);
    haylen::ui::Backend& backend = getUi().getBackend();
    backend.makeCurrent();
    ImGuiWindow* window = ImGui::FindWindowByName("Inspector");
    ASSERT_NE(window, nullptr);
    const ImGuiTable* table = ImGui::TableFindByID(window->GetID("##values"));
    ASSERT_NE(table, nullptr);
    const float row = (table->OuterRect.Max.y - table->OuterRect.Min.y) / 6.0F;
    const ImVec2 first{table->OuterRect.Min.x + 60.0F, table->OuterRect.Min.y + row * 0.5F};
    tap(place(first));
    tap(place({first.x, first.y + row}));
    ASSERT_GT(window->ScrollMax.y, 0.0F) << "The watched values show their rows.";

    EXPECT_EQ(measure(), quiet);
    EXPECT_EQ(getEngine().getError(), nullptr);
}

// An edit in the field of a value says what it changed in place of the hint, and a text that the setter refuses says why, which screen readers read from the tree of the UI.
TEST_F(InspectorWindowTest, TellsWhatAnEditChangedOrWhyItFailed) {
    getFixture().host().setAccessibilityTreeWanted(true);
    getFixture().runLua("values = {speed = 2} require('haylen.debug').watch('values', values) require('haylen.scene').push({}) require('haylen.debug').setInspectorVisible(true)");
    frames(2);
    EXPECT_EQ(readLine(), haylen::debug::InspectorWindow::kHint);

    haylen::ui::Backend& backend = getUi().getBackend();
    backend.makeCurrent();
    ImGuiWindow* window = ImGui::FindWindowByName("Inspector");
    ASSERT_NE(window, nullptr);
    const ImGuiTable* table = ImGui::TableFindByID(window->GetID("##values"));
    ASSERT_NE(table, nullptr);
    const float row = (table->OuterRect.Max.y - table->OuterRect.Min.y) / 6.0F;
    const ImVec2 first{table->OuterRect.Min.x + 60.0F, table->OuterRect.Min.y + row * 0.5F};
    tap(place(first));
    tap(place({first.x, first.y + row}));

    // The row of the speed comes under the root of the watched values and the watched table, with its field in the column of the values.
    const haylen::math::Vec2 field = place({table->OuterRect.Max.x - 20.0F, first.y + row * 2.0F});
    tap(field);
    type(U"5");
    key(haylen::input::Key::Enter);
    frames(2);
    EXPECT_EQ(readLine(), "Set \"watched.values.speed\" to 25.");
    EXPECT_EQ(getFixture().lua("return values.speed"), "25");

    // The line of the edit is shorter than the hint, so the rows move up and the field takes its place again.
    const ImGuiTable* moved = ImGui::TableFindByID(window->GetID("##values"));
    ASSERT_NE(moved, nullptr);
    tap(place({moved->OuterRect.Max.x - 20.0F, moved->OuterRect.Min.y + row * 2.5F}));
    type(U"x");
    key(haylen::input::Key::Enter);
    frames(2);
    EXPECT_EQ(readLine(), "The value at \"watched.values.speed\" is a number, and \"25x\" is not one.");
    EXPECT_EQ(getFixture().lua("return values.speed"), "25");
}

} // namespace haylen::lua::debug
