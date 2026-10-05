#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/text/Direction.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Collection.hpp"
#include "haylen/ui/CollectionCell.hpp"
#include "haylen/ui/CollectionItems.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Scaling.hpp"
#include "haylen/ui/Theme.hpp"
#include "support/TestFiles.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

// A screen the bars are measured on: its framebuffer, its pixels per point, the scaling of the design area and the scaling of the UI.
struct Screen {
    const char* name = "";
    math::Vec2 framebuffer;
    float pixelsPerPoint = 1.0F;
    graphics::Viewport::ScalingPolicy policy = graphics::Viewport::ScalingPolicy::Fit;
    Scaling scaling;
};

// Where the bar lies against the content: after it along the width, before it in a right-to-left UI, or below it.
enum class Side : std::uint8_t {
    End,
    Start,
    Below,
};

class ScrollbarTest : public ::testing::Test, public test::UiFixture {
  protected:
    // The bars draw in magenta and the selected rows of popups in cyan, which nothing else in these GUIs draws.
    static constexpr const char* kMarks = R"("style": {"colors": {"scrollbar": "#FF00FF", "scrollbarHover": "#FF00FF", "selection": "#00FFFF"}})";
    static constexpr std::uint32_t kBarColor = 0xFF00FFU;
    static constexpr std::uint32_t kRowColor = 0xFFFF00U;

    // Twenty lines of text, four of which a text area shows at once.
    static constexpr const char* kNotes = R"(Line 1\nLine 2\nLine 3\nLine 4\nLine 5\nLine 6\nLine 7\nLine 8\nLine 9\nLine 10\nLine 11\nLine 12\nLine 13\nLine 14\nLine 15\nLine 16\nLine 17\nLine 18\nLine 19\nLine 20)";

    // Design units on a desktop, twice as large, a window so small that the theme gap spans fewer than four points, a dense screen in the physical mode, the physical mode at half its size, and a design area that is not scaled on a dense screen.
    static constexpr std::array<Screen, 6> kScreens{{
        {.name = "design", .framebuffer = {1920.0F, 1080.0F}},
        {.name = "design 2x", .framebuffer = {1920.0F, 1080.0F}, .scaling = {.mode = Scaling::Mode::Design, .factor = 2.0F}},
        {.name = "small window", .framebuffer = {640.0F, 360.0F}},
        {.name = "physical", .framebuffer = {3840.0F, 2160.0F}, .pixelsPerPoint = 2.0F, .scaling = {.mode = Scaling::Mode::Physical, .factor = 1.0F}},
        {.name = "physical 0.5x", .framebuffer = {3840.0F, 2160.0F}, .pixelsPerPoint = 2.0F, .scaling = {.mode = Scaling::Mode::Physical, .factor = 0.5F}},
        {.name = "unscaled", .framebuffer = {2560.0F, 1440.0F}, .pixelsPerPoint = 2.0F, .policy = graphics::Viewport::ScalingPolicy::None},
    }};

    ScrollbarTest() : UiFixture({{"content/ui/thumb.png", toText(test::TestFiles::pngImage(12, 12, 0xFFFFFFFFU))}, {"content/ui/groove.png", toText(test::TestFiles::pngImage(12, 12, 0x808080FFU))}}) {
        // Without antialiasing every vertex of a bar lies on its outline, so the box of its vertices is the bar.
        getUi().getBackend().makeCurrent();
        ImGui::GetStyle().AntiAliasedFill = false;
    }

    [[nodiscard]] static std::string toText(const std::vector<std::uint8_t>& bytes) {
        return {bytes.begin(), bytes.end()};
    }

    [[nodiscard]] static std::string marked(const std::string& node) {
        return R"({"kind": "column", )" + std::string(kMarks) + R"(, "children": [)" + node + "]}";
    }

    [[nodiscard]] static std::string labels(int count) {
        std::string list;
        for (int index = 1; index <= count; ++index) {
            list += (index > 1 ? ", " : "") + std::string(R"({"kind": "label", "text": "Line )") + std::to_string(index) + R"("})";
        }
        return list;
    }

    void use(const Screen& screen) {
        getFixture().host().setDpiScale(screen.pixelsPerPoint);
        getFixture().host().resize(screen.framebuffer);
        getEngine().setScaling(screen.policy);
        getUi().setScaling(screen.scaling);
        frames(3);
    }

    // The points of the screen one UI unit spans, from the density of the design area, the pixels per point and the scale of the UI.
    [[nodiscard]] float getPointsPerUnit() {
        const math::Vec2 density = getEngine().getViewport().getPixelsPerUnit() * getUi().getBackend().getScale();
        return std::min(density.x, density.y) / getFixture().host().getDpiScale();
    }

    // Clicks a point given in UI units, which the pointer of the fixture takes in design units.
    void clickUi(math::Vec2 point) {
        click(point * getUi().getBackend().getScale());
    }

    // The box of the vertices ImGui drew in a color inside an area in the last frame.
    [[nodiscard]] static std::optional<math::Rect> findDrawn(std::uint32_t color, const math::Rect& area) {
        math::Vec2 low{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        math::Vec2 high{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
        bool found = false;
        const math::Rect reach = area.expanded(1.0F);
        for (const ImDrawList* list : ImGui::GetDrawData()->CmdLists) {
            for (const ImDrawVert& vertex : list->VtxBuffer) {
                const math::Vec2 point{vertex.pos.x, vertex.pos.y};
                if ((vertex.col & 0x00FFFFFFU) != color || (vertex.col >> 24U) == 0 || !reach.contains(point)) {
                    continue;
                }
                low = {std::min(low.x, point.x), std::min(low.y, point.y)};
                high = {std::max(high.x, point.x), std::max(high.y, point.y)};
                found = true;
            }
        }
        return found ? std::optional(math::Rect::fromMinMax(low, high)) : std::nullopt;
    }

    [[nodiscard]] static math::Rect findBar(const math::Rect& area) {
        const std::optional<math::Rect> bar = findDrawn(kBarColor, area);
        if (!bar) {
            throw std::logic_error("No scroll bar was drawn in the area.");
        }
        return *bar;
    }

    // The bar lies beside the content and keeps the gap of the theme from it, which never spans fewer than four points, and the inset from the edge of its area.
    void expectApart(const math::Rect& content, const math::Rect& bar, const math::Rect& area, Side side) {
        const Theme& theme = getUi().getTheme();
        const float points = getPointsPerUnit();
        const float gap = side == Side::Below ? bar.y - content.getBottom() : side == Side::End ? bar.x - content.getRight() : content.x - bar.getRight();
        const float inset = side == Side::Below ? area.getBottom() - bar.getBottom() : side == Side::End ? area.getRight() - bar.getRight() : bar.x - area.x;
        EXPECT_GE(gap * points, 4.0F - 0.01F);
        EXPECT_GE(gap, theme.getMetric(Theme::Metric::ScrollbarGap) - 0.01F);
        EXPECT_GE(inset, theme.getMetric(Theme::Metric::ScrollbarInset) - 0.01F);
        EXPECT_NEAR(side == Side::Below ? bar.height : bar.width, theme.getMetric(Theme::Metric::ScrollbarSize), 0.01F);
        EXPECT_FALSE(content.intersects(bar));
    }

    [[nodiscard]] static const math::Rect& getItemBounds(const Collection& collection, std::size_t index) {
        return collection.findCell(index)->getRoot().getBounds();
    }

    [[nodiscard]] static std::shared_ptr<CollectionItems> makeItems(std::size_t count) {
        std::vector<CollectionItems::Item> items(count);
        for (std::size_t index = 0; index < count; ++index) {
            items[index] = {.id = "i" + std::to_string(index), .type = {}, .fields = {{"title", "Item " + std::to_string(index)}}};
        }
        auto source = std::make_shared<CollectionItems>();
        source->assign(std::move(items));
        return source;
    }

    [[nodiscard]] static math::Rect merge(const std::vector<math::Rect>& pieces) {
        math::Rect box = pieces.front();
        for (const math::Rect& piece : pieces) {
            box = box.merged(piece);
        }
        return box;
    }

    // The clip of the text a field draws, which is where its text may show.
    [[nodiscard]] static math::Rect findTextClip(const math::Rect& field) {
        for (const ImDrawList* list : ImGui::GetDrawData()->CmdLists) {
            for (const ImDrawCmd& command : list->CmdBuffer) {
                const math::Rect clip = math::Rect::fromMinMax({command.ClipRect.x, command.ClipRect.y}, {command.ClipRect.z, command.ClipRect.w});
                if (command.UserCallback != nullptr && command.UserCallback != ImGui::GetPlatformIO().DrawCallback_ResetRenderState && field.contains(clip)) {
                    return clip;
                }
            }
        }
        throw std::logic_error("The field drew no text.");
    }
};

} // namespace

TEST_F(ScrollbarTest, KeepsTheBarOfAScrollInALaneBesideItsContentAtEveryScale) {
    auto gui = mount(marked(R"({"kind": "scroll", "id": "area", "width": 300, "height": 240, "children": [{"kind": "column", "id": "content", "children": [)" + labels(30) + "]}]}"));
    for (const Screen& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        const math::Rect area = getBounds(*gui, "area");
        expectApart(getBounds(*gui, "content"), findBar(area), area, Side::End);
    }

    // In a right-to-left UI the bar takes the left edge.
    use(kScreens[0]);
    getUi().setDirection(text::Direction::RightToLeft);
    frames(2);
    expectApart(getBounds(*gui, "content"), findBar(getBounds(*gui, "area")), getBounds(*gui, "area"), Side::Start);
    getUi().setDirection(text::Direction::LeftToRight);

    // Content that fits takes the whole width and shows no bar.
    gui->replaceChildren("content", core::Json::parse("[" + labels(2) + "]"));
    frames(2);
    EXPECT_FLOAT_EQ(getBounds(*gui, "content").width, getBounds(*gui, "area").width);
    EXPECT_FALSE(findDrawn(kBarColor, getBounds(*gui, "area")).has_value());
}

TEST_F(ScrollbarTest, TakesTheGapFromTheStyleOfANodeButNeverLessThanFourPoints) {
    // clang-format off
    auto gui = mount(marked(R"({"kind": "row", "children": [
        {"kind": "scroll", "id": "wide", "width": 300, "height": 240, "style": {"metrics": {"scrollbarGap": 20, "scrollbarSize": 14, "scrollbarInset": 6}}, "children": [{"kind": "column", "id": "wideContent", "children": [)" + labels(30) + R"(]}]},
        {"kind": "scroll", "id": "none", "width": 300, "height": 240, "style": {"metrics": {"scrollbarGap": 0}}, "children": [{"kind": "column", "id": "noneContent", "children": [)" + labels(30) + R"(]}]}
    ]})"));
    // clang-format on
    const math::Rect wide = getBounds(*gui, "wide");
    const math::Rect bar = findBar(wide);
    EXPECT_FLOAT_EQ(bar.x - getBounds(*gui, "wideContent").getRight(), 20.0F);
    EXPECT_FLOAT_EQ(bar.width, 14.0F);
    EXPECT_FLOAT_EQ(wide.getRight() - bar.getRight(), 6.0F);

    for (const Screen& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        const float gap = findBar(getBounds(*gui, "none")).x - getBounds(*gui, "noneContent").getRight();
        EXPECT_NEAR(gap * getPointsPerUnit(), 4.0F, 0.01F);
    }
}

TEST_F(ScrollbarTest, ScrollsAScrollWithTheThumbAndTheTrackOfItsBar) {
    auto gui = mount(marked(R"({"kind": "scroll", "id": "area", "width": 300, "height": 240, "children": [{"kind": "column", "id": "content", "children": [)" + labels(30) + "]}]}"));
    const math::Rect area = getBounds(*gui, "area");
    const math::Rect thumb = findBar(area);

    // Dragging the thumb down moves the content up, and a press on the track below the thumb moves it by one view.
    drag(thumb.getCenter(), thumb.getCenter() + math::Vec2{0.0F, 60.0F});
    const float dragged = getBounds(*gui, "content").y;
    EXPECT_LT(dragged, area.y - 60.0F);
    click({thumb.getCenter().x, area.getBottom() - 6.0F});
    frames(2);
    EXPECT_NEAR(getBounds(*gui, "content").y, dragged - area.height, 1.0F);
}

TEST_F(ScrollbarTest, PutsTheBarOfAHorizontalScrollBelowItsContent) {
    std::string buttons;
    for (int index = 1; index <= 20; ++index) {
        buttons += (index > 1 ? ", " : "") + std::string(R"({"kind": "button", "text": "Item )") + std::to_string(index) + R"("})";
    }
    auto gui = mount(marked(R"({"kind": "scroll", "id": "area", "axis": "horizontal", "width": 300, "align": "start", "children": [{"kind": "row", "id": "content", "children": [)" + buttons + "]}]}"));
    for (const Screen& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        const math::Rect area = getBounds(*gui, "area");
        const math::Rect content = getBounds(*gui, "content");
        expectApart(content, findBar(area), area, Side::Below);
        // The lane adds to the height of the scroll, so the content keeps the height it measures.
        EXPECT_FLOAT_EQ(content.height, getUi().getTheme().getMetric(Theme::Metric::ControlHeight));
    }
}

// A collection drew its bar over the end of its cells, which now end the gap before the lane of the bar.
TEST_F(ScrollbarTest, KeepsTheCellsOfACollectionOutOfTheLaneOfItsBar) {
    auto gui = mount(marked(R"({"kind": "collection", "id": "list", "width": 300, "height": 240, "gap": 0, "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 40}}}})"));
    Collection& list = gui->getCollection("list");
    list.setSource(makeItems(100));
    for (const Screen& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        const math::Rect area = list.getBounds();
        expectApart(getItemBounds(list, 0), findBar(area), area, Side::End);
    }

    // In a right-to-left UI the bar takes the left edge.
    use(kScreens[0]);
    getUi().setDirection(text::Direction::RightToLeft);
    frames(3);
    expectApart(getItemBounds(list, 0), findBar(list.getBounds()), list.getBounds(), Side::Start);

    // Items that fit release the lane.
    list.setSource(makeItems(2));
    frames(3);
    EXPECT_FLOAT_EQ(getItemBounds(list, 0).width, list.getBounds().width);
    EXPECT_FALSE(findDrawn(kBarColor, list.getBounds()).has_value());
}

TEST_F(ScrollbarTest, PutsTheBarOfAHorizontalCollectionBelowItsCells) {
    auto gui = mount(marked(R"({"kind": "collection", "id": "shelf", "axis": "horizontal", "width": 300, "types": {"tile": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "width": 120, "height": 80}}}})"));
    Collection& shelf = gui->getCollection("shelf");
    shelf.setSource(makeItems(40));
    for (const Screen& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        const math::Rect area = shelf.getBounds();
        const math::Rect cell = getItemBounds(shelf, 0);
        expectApart(cell, findBar(area), area, Side::Below);
        EXPECT_FLOAT_EQ(cell.height, 80.0F);
    }
}

TEST_F(ScrollbarTest, KeepsListsTreesAndTablesInAScrollApartFromItsBar) {
    // clang-format off
    auto gui = mount(marked(R"({"kind": "scroll", "id": "area", "width": 400, "height": 300, "children": [{"kind": "column", "children": [
        {"kind": "list", "id": "list", "items": [{"id": "a", "text": "Alpha"}, {"id": "b", "text": "Beta"}, {"id": "c", "text": "Gamma"}]},
        {"kind": "tree", "id": "tree", "items": [{"id": "tools", "text": "Tools", "children": [{"id": "hammer", "text": "Hammer"}]}, {"id": "food", "text": "Food"}]},
        {"kind": "table", "id": "table", "columns": [{"text": "Name"}, {"text": "Level"}], "rows": [{"id": "ana", "cells": ["Ana", 4]}, {"id": "bo", "cells": ["Bo", 7]}]}
    ]}]})"));
    // clang-format on
    for (const Screen& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        const math::Rect area = getBounds(*gui, "area");
        const math::Rect bar = findBar(area);
        for (const char* id : {"list", "tree", "table"}) {
            SCOPED_TRACE(id);
            expectApart(getBounds(*gui, id), bar, area, Side::End);
        }
    }
}

TEST_F(ScrollbarTest, KeepsTheTextOfATextAreaApartFromItsBar) {
    auto gui = mount(marked(R"({"kind": "textArea", "id": "notes", "width": 400, "rows": 4, "value": ")" + std::string(kNotes) + R"("})"));
    for (const Screen& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        const math::Rect field = getBounds(*gui, "notes");
        // The bar keeps the inset from the border of the field, which runs inside its bounds.
        const math::Rect inner = field.inset(math::Insets::uniform(getUi().getTheme().getMetric(Theme::Metric::BorderWidth)));
        expectApart(findTextClip(field), findBar(field), inner, Side::End);
    }
}

TEST_F(ScrollbarTest, ScrollsTheTextOfATextAreaWithItsBar) {
    auto gui = mount(marked(R"({"kind": "textArea", "id": "notes", "width": 400, "rows": 4, "value": ")" + std::string(kNotes) + R"("})"));
    frames(2);
    const math::Rect field = getBounds(*gui, "notes");
    const math::Rect thumb = findBar(field);

    // The thumb of a field that is not edited drags its text, which the field draws higher the further the thumb goes.
    graphics2d::Renderer& renderer = getEngine().getRenderer2D();
    float top = 0.0F;
    // clang-format off
    const std::uint64_t overlay = renderer.addCanvasOverlay([&top](graphics2d::Renderer& drawing) {
        drawing.visitDrawn([&top](const graphics2d::Renderer::Drawn& drawn) {
            if (drawn.text) {
                top = math::Geometry::bounds(drawn.corners).y;
            }
        });
    });
    // clang-format on
    frames();
    const float before = top;
    drag(thumb.getCenter(), {thumb.getCenter().x, field.getBottom()});
    renderer.removeCanvasOverlay(overlay);
    EXPECT_LT(top, before - field.height);
}

TEST_F(ScrollbarTest, ScrollsTheRowsOfALongComboBesideItsBar) {
    std::string items;
    for (int index = 1; index <= 120; ++index) {
        items += (index > 1 ? ", " : "") + std::string(R"({"id": "i)") + std::to_string(index) + R"(", "text": "Choice )" + std::to_string(index) + R"("})";
    }
    auto gui = mount(marked(R"({"kind": "combo", "id": "pick", "width": 300, "selected": "i1", "items": [)" + items + "]}"));
    for (const Screen& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        clickUi(getBounds(*gui, "pick").getCenter());
        frames(3);
        const math::Rect display = getUi().getBackend().getDisplayRect();
        const std::optional<math::Rect> row = findDrawn(kRowColor, display);
        ASSERT_TRUE(row.has_value());
        expectApart(*row, findBar(display), display, Side::End);
        clickUi(display.getMax() - math::Vec2{1.0F, 1.0F});
    }
}

// A long list hangs below a combo at the top of the display and above one at the bottom, and both stay inside the display, where their rows scroll.
TEST_F(ScrollbarTest, KeepsALongPopupInsideTheDisplayOnTheSideOfItsAnchorWithMoreRoom) {
    std::string items;
    for (int index = 1; index <= 120; ++index) {
        items += (index > 1 ? ", " : "") + std::string(R"({"id": "i)") + std::to_string(index) + R"(", "text": "Choice )" + std::to_string(index) + R"("})";
    }
    auto gui = mount(marked(R"({"kind": "column", "justify": "spaceBetween", "height": 1080, "children": [{"kind": "combo", "id": "top", "width": 300, "items": [)" + items + R"(]}, {"kind": "combo", "id": "bottom", "width": 300, "items": [)" + items + "]}]}"));
    const math::Rect display = getUi().getBackend().getDisplayRect();
    for (const char* id : {"top", "bottom"}) {
        SCOPED_TRACE(id);
        const math::Rect anchor = getBounds(*gui, id);
        click(anchor.getCenter());
        frames(3);
        ASSERT_EQ(GImGui->OpenPopupStack.Size, 1);
        const ImGuiWindow& window = *GImGui->OpenPopupStack[0].Window;
        const math::Rect popup{window.Pos.x, window.Pos.y, window.Size.x, window.Size.y};
        EXPECT_TRUE(display.contains(popup));
        EXPECT_TRUE(std::string(id) == "top" ? popup.y >= anchor.getBottom() : popup.getBottom() <= anchor.y);
        EXPECT_TRUE(findDrawn(kBarColor, popup).has_value());
        click(display.getMax() - math::Vec2{1.0F, 1.0F});
    }
}

TEST_F(ScrollbarTest, ScrollsTheContentOfATallPopoverBesideItsBar) {
    auto gui = mount(marked(R"({"kind": "popover", "id": "more", "text": "More", "contentWidth": 300, "children": [{"kind": "column", "id": "content", "children": [)" + labels(120) + "]}]}"));
    for (const Screen& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        clickUi(getBounds(*gui, "more").getCenter());
        frames(3);
        const math::Rect display = getUi().getBackend().getDisplayRect();
        expectApart(getBounds(*gui, "content"), findBar(display), display, Side::End);
        clickUi(display.getMax() - math::Vec2{1.0F, 1.0F});
    }
}

TEST_F(ScrollbarTest, ScrollsTheBodyOfATallDialogBesideItsBar) {
    auto gui = mount(marked(R"({"kind": "dialog", "id": "credits", "open": true, "title": "Credits", "buttons": [{"id": "close", "text": "Close"}], "children": [{"kind": "column", "id": "content", "children": [)" + labels(120) + "]}]}"));
    for (const Screen& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        frames(12);
        // The bar keeps the inset from the border of the dialog, which runs inside its bounds.
        const math::Rect frame = getBounds(*gui, "credits").inset(math::Insets::uniform(getUi().getTheme().getMetric(Theme::Metric::BorderWidth)));
        expectApart(getBounds(*gui, "content"), findBar(frame), frame, Side::End);
    }
}

TEST_F(ScrollbarTest, KeepsTheGapInImmediateWindowsAndTheirChildren) {
    // clang-format off
    getFixture().runLua(R"(
        local imgui = require('haylen.imgui')
        local scene = require('haylen.scene')
        local ui = require('haylen.ui')
        ui.setTheme(ui.addTheme({name = 'marked', colors = {scrollbar = '#FF00FF', scrollbarHover = '#FF00FF'}}, 'dark'))
        scene.push({renderUi = function()
            if imgui.beginWindow('Tool', {x = 20, y = 20, width = 300, height = 200}) then
                for index = 1, 30 do
                    imgui.text('Line ' .. index)
                end
            end
            imgui.endWindow()
            if imgui.beginWindow('Inspector', {x = 340, y = 20, width = 300, height = 300}) then
                if imgui.beginChild('entries', 0, 200) then
                    for index = 1, 30 do
                        imgui.text('Entry ' .. index)
                    end
                end
                imgui.endChild()
            end
            imgui.endWindow()
        end})
    )");
    // clang-format on
    for (const Screen& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        for (const char* name : {"Tool", "Inspector"}) {
            SCOPED_TRACE(name);
            ImGuiWindow* window = ImGui::FindWindowByName(name);
            ASSERT_NE(window, nullptr);
            if (std::string(name) == "Inspector") {
                window = window->DC.ChildWindows[0];
            }
            const math::Rect area{window->Pos.x, window->Pos.y, window->Size.x, window->Size.y};
            const math::Rect content = math::Rect::fromMinMax({window->WorkRect.Min.x, area.y}, {window->WorkRect.Max.x, area.getBottom()});
            expectApart(content, findBar(area), area, Side::End);
        }
    }
}

TEST_F(ScrollbarTest, KeepsTheGapForBarsDrawnWithThemeImages) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "style": {"surfaces": {"scrollbar": {"image": "ui/thumb.png", "slice": 4}, "scrollbarTrack": {"image": "ui/groove.png", "slice": 4}}}, "children": [
        {"kind": "scroll", "id": "area", "width": 300, "height": 240, "children": [{"kind": "column", "id": "content", "children": [)" + labels(30) + R"(]}]}
    ]})");
    // clang-format on
    frames(5);
    // The renderer names the texture of a draw on its first quad, so the quads after it belong to the same image.
    std::vector<math::Rect> thumbs;
    std::vector<math::Rect> grooves;
    std::vector<math::Rect>* image = nullptr;
    graphics2d::Renderer& renderer = getEngine().getRenderer2D();
    // clang-format off
    const std::uint64_t overlay = renderer.addCanvasOverlay([&](graphics2d::Renderer& drawing) {
        drawing.visitDrawn([&](const graphics2d::Renderer::Drawn& drawn) {
            if (drawn.text || !drawn.label.empty()) {
                image = drawn.label == "ui/thumb.png" ? &thumbs : drawn.label == "ui/groove.png" ? &grooves : nullptr;
            }
            if (image != nullptr) {
                image->push_back(math::Geometry::bounds(drawn.corners));
            }
        });
    });
    // clang-format on
    frames();
    renderer.removeCanvasOverlay(overlay);
    ASSERT_FALSE(thumbs.empty());
    ASSERT_FALSE(grooves.empty());

    const math::Rect area = getBounds(*gui, "area");
    expectApart(getBounds(*gui, "content"), merge(grooves), area, Side::End);
    EXPECT_TRUE(merge(grooves).contains(merge(thumbs)));
}

} // namespace haylen::ui
