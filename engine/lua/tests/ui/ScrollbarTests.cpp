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
#include "haylen/math/Geometry2D.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/text/Direction.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Collection.hpp"
#include "haylen/ui/CollectionCell.hpp"
#include "haylen/ui/CollectionItems.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Scaling.hpp"
#include "haylen/ui/Scrollbar.hpp"
#include "haylen/ui/Theme.hpp"
#include "support/LuaUiFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::ui {

class ScrollbarTest : public ::testing::Test, public test::LuaUiFixture {
  protected:
    // A screen the bars are measured on: its framebuffer, its pixels per point, the scaling of the design area and the scaling of the UI.
    struct Display {
        const char* name = "";
        haylen::math::Vec2 framebuffer;
        float pixelsPerPoint = 1.0F;
        haylen::graphics::Viewport::ScalingPolicy policy = haylen::graphics::Viewport::ScalingPolicy::Fit;
        haylen::ui::Scaling scaling{};
    };

    // Where the bar lies against the content: after it along the width, before it in a right-to-left UI, or below it.
    enum class Side : std::uint8_t {
        End,
        Start,
        Below,
    };

    // The bars draw in magenta and the selected rows of popups in cyan, which nothing else in these GUIs draws, and the tests that mark a groove mark it in green.
    static constexpr const char* kMarks = R"("style": {"colors": {"scrollbar": "#FF00FF", "scrollbarHover": "#FF00FF", "selection": "#00FFFF"}})";
    static constexpr std::uint32_t kBarColor = 0xFF00FFU;
    static constexpr std::uint32_t kRowColor = 0xFFFF00U;
    static constexpr std::uint32_t kGrooveColor = 0x00FF00U;

    // Twenty lines of text, four of which a text area shows at once.
    static constexpr const char* kNotes = R"(Line 1\nLine 2\nLine 3\nLine 4\nLine 5\nLine 6\nLine 7\nLine 8\nLine 9\nLine 10\nLine 11\nLine 12\nLine 13\nLine 14\nLine 15\nLine 16\nLine 17\nLine 18\nLine 19\nLine 20)";

    // Design units on a desktop, twice as large, a window so small that the theme gap spans fewer than four points, a dense screen in the physical mode, the physical mode at half its size, and a design area that is not scaled on a dense screen.
    static const std::array<Display, 6> kScreens;

    ScrollbarTest() : LuaUiFixture({{"content/ui/thumb.png", test::TestFiles::toText(test::TestFiles::pngImage(12, 12, 0xFFFFFFFFU))}, {"content/ui/groove.png", test::TestFiles::toText(test::TestFiles::pngImage(12, 12, 0x808080FFU))}}) {
        // Without antialiasing every vertex of a bar lies on its outline, so the box of its vertices is the bar.
        getUi().getBackend().makeCurrent();
        ImGui::GetStyle().AntiAliasedFill = false;
    }

    [[nodiscard]] static std::string marked(const std::string& node) {
        return R"({"kind": "column", )" + std::string(kMarks) + R"(, "children": [)" + node + "]}";
    }

    void use(const Display& screen) {
        getFixture().host().setDpiScale(screen.pixelsPerPoint);
        getFixture().host().resize(screen.framebuffer);
        getEngine().setScaling(screen.policy);
        getUi().setScaling(screen.scaling);
        frames(3);
    }

    // The points of the screen one UI unit spans, from the density of the design area, the pixels per point and the scale of the UI.
    [[nodiscard]] float getPointsPerUnit() {
        const haylen::math::Vec2 density = getEngine().getViewport().getPixelsPerUnit() * getUi().getBackend().getScale();
        return std::min(density.x, density.y) / getFixture().host().getDpiScale();
    }

    // Writes the top of the last text that each frame draws, the lines of a text area in these tests, until the overlay it returns is removed.
    std::uint64_t followTextTop(float& top) {
        // clang-format off
        return getEngine().getRenderer2D().addCanvasOverlay([&top](haylen::graphics2d::Renderer& drawing) {
            drawing.visitDrawn([&top](const haylen::graphics2d::Renderer::Drawn& drawn) {
                if (drawn.text) {
                    top = haylen::math::Geometry2D::bounds(drawn.corners).y;
                }
            });
        });
        // clang-format on
    }

    // Clicks a point given in UI units, which the pointer of the fixture takes in design units.
    void clickUi(haylen::math::Vec2 point) {
        click(point * getUi().getBackend().getScale());
    }

    // The box of the vertices ImGui drew and the shapes the UI drew in a color inside an area in the last frame.
    [[nodiscard]] std::optional<haylen::math::Rect> findDrawn(std::uint32_t color, const haylen::math::Rect& area) {
        haylen::math::Vec2 low{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        haylen::math::Vec2 high{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
        bool found = false;
        const haylen::math::Rect reach = area.expanded(1.0F);
        // clang-format off
        const auto add = [&](haylen::math::Vec2 from, haylen::math::Vec2 to) {
            low = haylen::math::Vec2::min(low, from);
            high = haylen::math::Vec2::max(high, to);
            found = true;
        };
        // clang-format on
        for (const ImDrawList* list : getDrawData(getUi()).CmdLists) {
            for (const ImDrawVert& vertex : list->VtxBuffer) {
                const haylen::math::Vec2 point{vertex.pos.x, vertex.pos.y};
                if ((vertex.col & 0x00FFFFFFU) == color && (vertex.col >> 24U) != 0 && reach.contains(point)) {
                    add(point, point);
                }
            }
        }

        for (const haylen::graphics2d::Shape& shape : getUi().getBackend().getShapes()) {
            const std::uint32_t packed = shape.color.toRgba8();
            const haylen::math::Rect inside = shape.bounds.intersection(reach);
            if ((packed & 0x00FFFFFFU) == color && (packed >> 24U) != 0 && inside.width > 0.0F && inside.height > 0.0F) {
                add(inside.getMin(), inside.getMax());
            }
        }

        return found ? std::optional(haylen::math::Rect::fromMinMax(low, high)) : std::nullopt;
    }

    [[nodiscard]] haylen::math::Rect findBar(const haylen::math::Rect& area) {
        const std::optional<haylen::math::Rect> bar = findDrawn(kBarColor, area);
        if (!bar) {
            throw std::logic_error("No scroll bar was drawn in the area.");
        }

        return *bar;
    }

    // The bar lies beside the content and keeps the gap of the theme from it, which never spans fewer than four points, and the inset from the edge of its area.
    void expectApart(const haylen::math::Rect& content, const haylen::math::Rect& bar, const haylen::math::Rect& area, Side side) {
        const haylen::ui::Theme& theme = getUi().getTheme();
        const float points = getPointsPerUnit();
        const float gap = side == Side::Below ? bar.y - content.getBottom() : side == Side::End ? bar.x - content.getRight() : content.x - bar.getRight();
        const float inset = side == Side::Below ? area.getBottom() - bar.getBottom() : side == Side::End ? area.getRight() - bar.getRight() : bar.x - area.x;
        EXPECT_GE(gap * points, 4.0F - 0.01F);
        EXPECT_GE(gap, theme.getMetric(haylen::ui::Theme::Metric::ScrollbarGap) - 0.01F);
        EXPECT_GE(inset, theme.getMetric(haylen::ui::Theme::Metric::ScrollbarInset) - 0.01F);
        EXPECT_NEAR(side == Side::Below ? bar.height : bar.width, theme.getMetric(haylen::ui::Theme::Metric::ScrollbarSize), 0.01F);
        EXPECT_FALSE(content.intersects(bar));
    }

    [[nodiscard]] static const haylen::math::Rect& getItemBounds(const haylen::ui::Collection& collection, std::size_t index) {
        const haylen::ui::CollectionCell* cell = collection.findCell(index);
        if (cell == nullptr) {
            throw std::logic_error("The collection shows no cell for the item " + std::to_string(index) + ".");
        }

        return cell->getRoot().getBounds();
    }

    [[nodiscard]] static std::shared_ptr<haylen::ui::CollectionItems> makeItems(std::size_t count) {
        std::vector<haylen::ui::CollectionItems::Item> items(count);
        for (std::size_t index = 0; index < count; ++index) {
            items[index] = {.id = "i" + std::to_string(index), .type = {}, .fields = {{"title", "Item " + std::to_string(index)}}};
        }

        auto source = std::make_shared<haylen::ui::CollectionItems>();
        source->assign(std::move(items));
        return source;
    }

    [[nodiscard]] static haylen::math::Rect merge(const std::vector<haylen::math::Rect>& pieces) {
        haylen::math::Rect box = pieces.front();
        for (const haylen::math::Rect& piece : pieces) {
            box = box.merged(piece);
        }

        return box;
    }

    // The clip of the text a field draws, which is where its text may show.
    [[nodiscard]] haylen::math::Rect findTextClip(const haylen::math::Rect& field) {
        for (const ImDrawList* list : getDrawData(getUi()).CmdLists) {
            for (const ImDrawCmd& command : list->CmdBuffer) {
                const haylen::math::Rect clip = haylen::math::Rect::fromMinMax({command.ClipRect.x, command.ClipRect.y}, {command.ClipRect.z, command.ClipRect.w});
                if (command.UserCallback != nullptr && command.UserCallback != ImGui::GetPlatformIO().DrawCallback_ResetRenderState && field.contains(clip)) {
                    return clip;
                }
            }
        }

        throw std::logic_error("The field drew no text.");
    }
};

const std::array<ScrollbarTest::Display, 6> ScrollbarTest::kScreens{{
    {.name = "design", .framebuffer = {1920.0F, 1080.0F}},
    {.name = "design 2x", .framebuffer = {1920.0F, 1080.0F}, .scaling = {.mode = haylen::ui::Scaling::Mode::Design, .factor = 2.0F}},
    {.name = "small window", .framebuffer = {640.0F, 360.0F}},
    {.name = "physical", .framebuffer = {3840.0F, 2160.0F}, .pixelsPerPoint = 2.0F, .scaling = {.mode = haylen::ui::Scaling::Mode::Physical, .factor = 1.0F}},
    {.name = "physical 0.5x", .framebuffer = {3840.0F, 2160.0F}, .pixelsPerPoint = 2.0F, .scaling = {.mode = haylen::ui::Scaling::Mode::Physical, .factor = 0.5F}},
    {.name = "unscaled", .framebuffer = {2560.0F, 1440.0F}, .pixelsPerPoint = 2.0F, .policy = haylen::graphics::Viewport::ScalingPolicy::None},
}};

TEST_F(ScrollbarTest, KeepsTheBarOfAScrollInALaneBesideItsContentAtEveryScale) {
    auto gui = mount(marked(R"({"kind": "scroll", "id": "area", "scrollbar": true, "width": 300, "height": 240, "children": [{"kind": "column", "id": "content", "children": [)" + labels(30) + "]}]}"));
    for (const Display& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        const haylen::math::Rect area = getBounds(*gui, "area");
        expectApart(getBounds(*gui, "content"), findBar(area), area, Side::End);
    }

    // In a right-to-left UI the bar takes the left edge.
    use(kScreens[0]);
    getUi().setDirection(haylen::text::Direction::RightToLeft);
    frames(2);
    expectApart(getBounds(*gui, "content"), findBar(getBounds(*gui, "area")), getBounds(*gui, "area"), Side::Start);
    getUi().setDirection(haylen::text::Direction::LeftToRight);

    // Content that fits takes the whole width and shows no bar.
    gui->replaceChildren("content", haylen::core::Json::parse("[" + labels(2) + "]"));
    frames(2);
    EXPECT_FLOAT_EQ(getBounds(*gui, "content").width, getBounds(*gui, "area").width);
    EXPECT_FALSE(findDrawn(kBarColor, getBounds(*gui, "area")).has_value());
}

// Areas that scroll show no bar unless the app asks for one, so their content takes the whole area and a text area keeps the lane for its lines.
TEST_F(ScrollbarTest, DrawsNoBarUnlessTheAppAsksForOne) {
    // clang-format off
    auto gui = mount(marked(R"({"kind": "row", "align": "start", "children": [
        {"kind": "scroll", "id": "area", "width": 300, "height": 240, "children": [{"kind": "column", "id": "content", "children": [)" + labels(30) + R"(]}]},
        {"kind": "collection", "id": "list", "width": 300, "height": 240, "gap": 0, "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 40}}}},
        {"kind": "column", "children": [
            {"kind": "textArea", "id": "plain", "width": 400, "rows": 4, "value": ")" + std::string(kNotes) + R"("},
            {"kind": "textArea", "id": "barred", "scrollbar": true, "width": 400, "rows": 4, "value": ")" + std::string(kNotes) + R"("}
        ]}
    ]})"));
    // clang-format on
    haylen::ui::Collection& list = gui->getCollection("list");
    list.setSource(makeItems(100));
    frames(3);

    const haylen::math::Rect area = getBounds(*gui, "area");
    EXPECT_FLOAT_EQ(getBounds(*gui, "content").width, area.width);
    EXPECT_FALSE(findDrawn(kBarColor, area).has_value());
    EXPECT_FLOAT_EQ(getItemBounds(list, 0).width, list.getBounds().width);
    EXPECT_FALSE(findDrawn(kBarColor, list.getBounds()).has_value());

    const haylen::math::Rect plain = getBounds(*gui, "plain");
    const haylen::math::Rect barred = getBounds(*gui, "barred");
    EXPECT_FALSE(findDrawn(kBarColor, plain).has_value());
    EXPECT_TRUE(findDrawn(kBarColor, barred).has_value());
    EXPECT_GE(findTextClip(plain).width - findTextClip(barred).width, haylen::ui::Scrollbar::getLane(getUi().getContext()) - 0.5F);
}

// A finger scrolls the lines of a text area that shows no bar, and lets go of the field, which does not start editing.
TEST_F(ScrollbarTest, ScrollsTheTextOfATextAreaWithAFinger) {
    auto gui = mount(marked(R"({"kind": "textArea", "id": "notes", "width": 400, "rows": 4, "value": ")" + std::string(kNotes) + R"("})"));
    frames(2);
    const haylen::math::Rect field = getBounds(*gui, "notes");
    float top = 0.0F;
    const std::uint64_t overlay = followTextTop(top);
    frames();
    const float before = top;

    swipe(field.getCenter() + haylen::math::Vec2{0.0F, 40.0F}, field.getCenter() - haylen::math::Vec2{0.0F, 60.0F});
    getEngine().getRenderer2D().removeCanvasOverlay(overlay);
    EXPECT_LT(top, before - 60.0F);
    EXPECT_FALSE(ImGui::GetIO().WantTextInput);
}

// A flat theme draws the groove the thumb runs in, as wide as the thumb and along the whole bar, in the track color of scroll bars.
TEST_F(ScrollbarTest, RunsTheThumbInAGrooveOfTheTrackColor) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "style": {"colors": {"scrollbar": "#FF00FF", "scrollbarHover": "#FF00FF", "scrollbarTrack": "#00FF00"}}, "children": [
        {"kind": "scroll", "id": "area", "scrollbar": true, "width": 300, "height": 240, "children": [{"kind": "column", "id": "content", "children": [)" + labels(30) + R"(]}]}
    ]})");
    // clang-format on
    const haylen::math::Rect area = getBounds(*gui, "area");
    const haylen::math::Rect thumb = findBar(area);
    const std::optional<haylen::math::Rect> groove = findDrawn(0x00FF00U, area);
    ASSERT_TRUE(groove.has_value());
    expectApart(getBounds(*gui, "content"), *groove, area, Side::End);
    EXPECT_TRUE(groove->contains(thumb));
    EXPECT_FLOAT_EQ(groove->width, thumb.width);
    EXPECT_GT(groove->height, thumb.height * 2.0F);
}

TEST_F(ScrollbarTest, TakesTheGapFromTheStyleOfANodeButNeverLessThanFourPoints) {
    // clang-format off
    auto gui = mount(marked(R"({"kind": "row", "children": [
        {"kind": "scroll", "id": "wide", "scrollbar": true, "width": 300, "height": 240, "style": {"metrics": {"scrollbarGap": 20, "scrollbarSize": 14, "scrollbarInset": 6}}, "children": [{"kind": "column", "id": "wideContent", "children": [)" + labels(30) + R"(]}]},
        {"kind": "scroll", "id": "none", "scrollbar": true, "width": 300, "height": 240, "style": {"metrics": {"scrollbarGap": 0}}, "children": [{"kind": "column", "id": "noneContent", "children": [)" + labels(30) + R"(]}]}
    ]})"));
    // clang-format on
    const haylen::math::Rect wide = getBounds(*gui, "wide");
    const haylen::math::Rect bar = findBar(wide);
    EXPECT_FLOAT_EQ(bar.x - getBounds(*gui, "wideContent").getRight(), 20.0F);
    EXPECT_FLOAT_EQ(bar.width, 14.0F);
    EXPECT_FLOAT_EQ(wide.getRight() - bar.getRight(), 6.0F);

    for (const Display& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        const float gap = findBar(getBounds(*gui, "none")).x - getBounds(*gui, "noneContent").getRight();
        EXPECT_NEAR(gap * getPointsPerUnit(), 4.0F, 0.01F);
    }
}

TEST_F(ScrollbarTest, ScrollsAScrollWithTheThumbAndTheTrackOfItsBar) {
    auto gui = mount(marked(R"({"kind": "scroll", "id": "area", "scrollbar": true, "width": 300, "height": 240, "children": [{"kind": "column", "id": "content", "children": [)" + labels(30) + "]}]}"));
    const haylen::math::Rect area = getBounds(*gui, "area");
    const haylen::math::Rect thumb = findBar(area);

    // Dragging the thumb down moves the content up, and a press on the track below the thumb moves it by one view.
    drag(thumb.getCenter(), thumb.getCenter() + haylen::math::Vec2{0.0F, 60.0F});
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

    auto gui = mount(marked(R"({"kind": "scroll", "id": "area", "scrollbar": true, "axis": "horizontal", "width": 300, "align": "start", "children": [{"kind": "row", "id": "content", "children": [)" + buttons + "]}]}"));
    for (const Display& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        const haylen::math::Rect area = getBounds(*gui, "area");
        const haylen::math::Rect content = getBounds(*gui, "content");
        expectApart(content, findBar(area), area, Side::Below);
        // The lane adds to the height of the scroll, so the content keeps the height it measures.
        EXPECT_FLOAT_EQ(content.height, getUi().getTheme().getMetric(haylen::ui::Theme::Metric::ControlHeight));
    }
}

// A collection drew its bar over the end of its cells, which now end the gap before the lane of the bar.
TEST_F(ScrollbarTest, KeepsTheCellsOfACollectionOutOfTheLaneOfItsBar) {
    auto gui = mount(marked(R"({"kind": "collection", "id": "list", "scrollbar": true, "width": 300, "height": 240, "gap": 0, "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 40}}}})"));
    haylen::ui::Collection& list = gui->getCollection("list");
    list.setSource(makeItems(100));
    for (const Display& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        const haylen::math::Rect area = list.getBounds();
        expectApart(getItemBounds(list, 0), findBar(area), area, Side::End);
    }

    // In a right-to-left UI the bar takes the left edge.
    use(kScreens[0]);
    getUi().setDirection(haylen::text::Direction::RightToLeft);
    frames(3);
    expectApart(getItemBounds(list, 0), findBar(list.getBounds()), list.getBounds(), Side::Start);

    // Items that fit release the lane.
    list.setSource(makeItems(2));
    frames(3);
    EXPECT_FLOAT_EQ(getItemBounds(list, 0).width, list.getBounds().width);
    EXPECT_FALSE(findDrawn(kBarColor, list.getBounds()).has_value());
}

TEST_F(ScrollbarTest, PutsTheBarOfAHorizontalCollectionBelowItsCells) {
    auto gui = mount(marked(R"({"kind": "collection", "id": "shelf", "scrollbar": true, "axis": "horizontal", "width": 300, "types": {"tile": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "width": 120, "height": 80}}}})"));
    haylen::ui::Collection& shelf = gui->getCollection("shelf");
    shelf.setSource(makeItems(40));
    for (const Display& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        const haylen::math::Rect area = shelf.getBounds();
        const haylen::math::Rect cell = getItemBounds(shelf, 0);
        expectApart(cell, findBar(area), area, Side::Below);
        EXPECT_FLOAT_EQ(cell.height, 80.0F);
    }
}

TEST_F(ScrollbarTest, KeepsListsTreesAndTablesInAScrollApartFromItsBar) {
    // clang-format off
    auto gui = mount(marked(R"({"kind": "scroll", "id": "area", "scrollbar": true, "width": 400, "height": 300, "children": [{"kind": "column", "children": [
        {"kind": "list", "id": "list", "items": [{"id": "a", "text": "Alpha"}, {"id": "b", "text": "Beta"}, {"id": "c", "text": "Gamma"}]},
        {"kind": "tree", "id": "tree", "items": [{"id": "tools", "text": "Tools", "children": [{"id": "hammer", "text": "Hammer"}]}, {"id": "food", "text": "Food"}]},
        {"kind": "table", "id": "table", "columns": [{"text": "Name"}, {"text": "Level"}], "rows": [{"id": "ana", "cells": ["Ana", 4]}, {"id": "bo", "cells": ["Bo", 7]}]}
    ]}]})"));
    // clang-format on
    for (const Display& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        const haylen::math::Rect area = getBounds(*gui, "area");
        const haylen::math::Rect bar = findBar(area);
        for (const char* id : {"list", "tree", "table"}) {
            SCOPED_TRACE(id);
            expectApart(getBounds(*gui, id), bar, area, Side::End);
        }
    }
}

TEST_F(ScrollbarTest, KeepsTheTextOfATextAreaApartFromItsBar) {
    auto gui = mount(marked(R"({"kind": "textArea", "id": "notes", "scrollbar": true, "width": 400, "rows": 4, "value": ")" + std::string(kNotes) + R"("})"));
    for (const Display& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);
        const haylen::math::Rect field = getBounds(*gui, "notes");
        // The bar keeps the inset from the border of the field, which runs inside its bounds.
        const haylen::math::Rect inner = field.inset(haylen::math::Insets::uniform(getUi().getTheme().getMetric(haylen::ui::Theme::Metric::BorderWidth)));
        expectApart(findTextClip(field), findBar(field), inner, Side::End);
    }
}

TEST_F(ScrollbarTest, ScrollsTheTextOfATextAreaWithItsBar) {
    auto gui = mount(marked(R"({"kind": "textArea", "id": "notes", "scrollbar": true, "width": 400, "rows": 4, "value": ")" + std::string(kNotes) + R"("})"));
    frames(2);
    const haylen::math::Rect field = getBounds(*gui, "notes");
    const haylen::math::Rect thumb = findBar(field);

    // The thumb of a field that is not edited drags its text, which the field draws higher the further the thumb goes.
    haylen::graphics2d::Renderer& renderer = getEngine().getRenderer2D();
    float top = 0.0F;
    const std::uint64_t overlay = followTextTop(top);
    frames();
    const float before = top;
    drag(thumb.getCenter(), {thumb.getCenter().x, field.getBottom()});
    renderer.removeCanvasOverlay(overlay);
    EXPECT_LT(top, before - field.height);
}

// A text area fades its lines out along the edge where more of them continue, like the other areas that scroll, and only there, over the box its text is cut to.
TEST_F(ScrollbarTest, FadesTheTextOfATextAreaWhereItContinues) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "textArea", "id": "notes", "width": 400, "rows": 4, "value": ")" + std::string(kNotes) + R"("}]})");
    frames(2);
    const haylen::math::Rect field = getBounds(*gui, "notes");
    const haylen::math::Rect clip = findTextClip(field);
    ASSERT_EQ(getUi().getBackend().getFades().size(), 1U);
    const haylen::ui::Backend::Fade below = getUi().getBackend().getFades().front();
    EXPECT_EQ(below.area, clip);
    EXPECT_EQ(below.edges.top, 0.0F);
    EXPECT_EQ(below.edges.bottom, 1.0F);

    wheel(field.getCenter(), -40.0F);
    frames(60);
    ASSERT_EQ(getUi().getBackend().getFades().size(), 1U);
    EXPECT_EQ(getUi().getBackend().getFades().front().edges.top, 1.0F);
}

// A text area has no motion of its own, so the momentum that the system adds to a trackpad once its fingers left keeps scrolling its text.
TEST_F(ScrollbarTest, ScrollsATextAreaWithTheMomentumOfATrackpad) {
    using Phase = haylen::platform::Event::ScrollPhase;
    auto gui = mount(marked(R"({"kind": "textArea", "id": "notes", "width": 400, "rows": 4, "value": ")" + std::string(kNotes) + R"("})"));
    frames(2);
    const haylen::math::Vec2 inside = getBounds(*gui, "notes").getCenter();
    haylen::graphics2d::Renderer& renderer = getEngine().getRenderer2D();
    float top = 0.0F;
    const std::uint64_t overlay = followTextTop(top);
    // clang-format off
    const auto scroll = [&](float distance, Phase phase) {
        pointer(haylen::platform::Event::Type::MouseMove, inside);
        haylen::platform::Event event;
        event.type = haylen::platform::Event::Type::MouseScroll;
        event.scroll = {0.0F, distance};
        event.preciseScroll = true;
        event.scrollPhase = phase;
        getEngine().handleEvent(event);
        frames();
    };
    // clang-format on
    scroll(0.0F, Phase::Began);
    scroll(-10.0F, Phase::Changed);
    scroll(0.0F, Phase::Ended);
    const float lifted = top;

    for (int step = 0; step < 5; ++step) {
        scroll(-10.0F, Phase::Momentum);
    }

    renderer.removeCanvasOverlay(overlay);
    EXPECT_NEAR(top, lifted - 50.0F, 1.0F);
}

// A step of a wheel glides the lines of a text area to their place, as it glides every other area that scrolls.
TEST_F(ScrollbarTest, GlidesTheTextOfATextAreaByTheStepsOfAWheel) {
    auto gui = mount(marked(R"({"kind": "textArea", "id": "notes", "width": 400, "rows": 4, "value": ")" + std::string(kNotes) + R"("})"));
    frames(2);
    float top = 0.0F;
    const std::uint64_t overlay = followTextTop(top);
    frames();
    const float before = top;

    // The wheel reaches the area in the frame after it turns.
    wheel(getBounds(*gui, "notes").getCenter(), -1.0F);
    frames();
    const float started = before - top;
    frames(60);
    const float settled = before - top;
    getEngine().getRenderer2D().removeCanvasOverlay(overlay);
    EXPECT_GT(started, 0.0F);
    EXPECT_LT(started, settled * 0.5F);
    EXPECT_NEAR(settled, getUi().getTheme().getMetric(haylen::ui::Theme::Metric::ScrollLine), 0.5F);
}

// A popup scrolls without a bar: its rows take the whole width inside its padding, fade out along the edge where more of them continue, and move with the wheel, a finger and the focus.
TEST_F(ScrollbarTest, ScrollsTheRowsOfALongComboWithoutABar) {
    std::string items;
    for (int index = 1; index <= 120; ++index) {
        items += (index > 1 ? ", " : "") + std::string(R"({"id": "i)") + std::to_string(index) + R"(", "text": "Choice )" + std::to_string(index) + R"("})";
    }

    auto gui = mount(marked(R"({"kind": "combo", "id": "pick", "width": 300, "selected": "i1", "items": [)" + items + "]}"));
    click(getBounds(*gui, "pick").getCenter());
    frames(3);
    ASSERT_EQ(GImGui->OpenPopupStack.Size, 1);
    const ImGuiWindow& window = *GImGui->OpenPopupStack[0].Window;
    const haylen::math::Rect popup{window.Pos.x, window.Pos.y, window.Size.x, window.Size.y};
    const std::optional<haylen::math::Rect> row = findDrawn(kRowColor, popup);
    ASSERT_TRUE(row.has_value());
    EXPECT_NEAR(row->width, popup.width - getUi().getTheme().getMetric(haylen::ui::Theme::Metric::MenuPadding) * 2.0F, 1.0F);
    EXPECT_FALSE(findDrawn(kBarColor, popup).has_value());
    ASSERT_EQ(getUi().getBackend().getFades().size(), 1U);
    EXPECT_EQ(getUi().getBackend().getFades().front().edges.top, 0.0F);
    EXPECT_EQ(getUi().getBackend().getFades().front().edges.bottom, 1.0F);

    wheel(popup.getCenter(), -1.0F);
    frames(30);
    const float wheeled = window.Scroll.y;
    EXPECT_NEAR(wheeled, getUi().getTheme().getMetric(haylen::ui::Theme::Metric::ScrollLine), 1.0F);

    // A finger that drags the rows scrolls them and picks none of them.
    clearEvents();
    swipe(popup.getCenter(), popup.getCenter() - haylen::math::Vec2{0.0F, 200.0F});
    ASSERT_EQ(GImGui->OpenPopupStack.Size, 1);
    EXPECT_GT(window.Scroll.y, wheeled + 150.0F);
    EXPECT_TRUE(getEventNames().empty());

    // The focus brings the row it moves to into view, and the scroll keeps it there.
    const float swiped = window.Scroll.y;
    for (int step = 0; step < 60; ++step) {
        key(haylen::input::Key::Down);
    }

    frames(30);
    EXPECT_GT(window.Scroll.y, swiped + 200.0F);
}

// A long list hangs below a combo at the top of the display and above one at the bottom, and both stay inside the display, where their rows scroll.
TEST_F(ScrollbarTest, KeepsALongPopupInsideTheDisplayOnTheSideOfItsAnchorWithMoreRoom) {
    std::string items;
    for (int index = 1; index <= 120; ++index) {
        items += (index > 1 ? ", " : "") + std::string(R"({"id": "i)") + std::to_string(index) + R"(", "text": "Choice )" + std::to_string(index) + R"("})";
    }

    auto gui = mount(marked(R"({"kind": "column", "justify": "spaceBetween", "height": 1080, "children": [{"kind": "combo", "id": "top", "width": 300, "items": [)" + items + R"(]}, {"kind": "combo", "id": "bottom", "width": 300, "items": [)" + items + "]}]}"));
    const haylen::math::Rect display = getUi().getBackend().getDisplayRect();
    for (const char* id : {"top", "bottom"}) {
        SCOPED_TRACE(id);
        const haylen::math::Rect anchor = getBounds(*gui, id);
        click(anchor.getCenter());
        frames(3);
        ASSERT_EQ(GImGui->OpenPopupStack.Size, 1);
        const ImGuiWindow& window = *GImGui->OpenPopupStack[0].Window;
        const haylen::math::Rect popup{window.Pos.x, window.Pos.y, window.Size.x, window.Size.y};
        EXPECT_TRUE(display.contains(popup));
        EXPECT_TRUE(std::string(id) == "top" ? popup.y >= anchor.getBottom() : popup.getBottom() <= anchor.y);
        EXPECT_FALSE(findDrawn(kBarColor, popup).has_value());
        click(display.getMax() - haylen::math::Vec2{1.0F, 1.0F});
    }
}

// The body of a dialog scrolls without a bar, at the whole width inside the padding of the dialog, with the wheel and with a finger.
TEST_F(ScrollbarTest, ScrollsTheBodyOfATallDialogWithoutABar) {
    auto gui = mount(marked(R"({"kind": "dialog", "id": "credits", "title": "Credits", "buttons": [{"id": "close", "text": "Close"}], "children": [{"kind": "column", "id": "content", "children": [)" + labels(120) + "]}]}"));
    frames(12);
    const haylen::math::Rect frame = getBounds(*gui, "credits");
    EXPECT_NEAR(getBounds(*gui, "content").width, frame.width - getUi().getTheme().getMetric(haylen::ui::Theme::Metric::PanelPadding) * 2.0F, 0.5F);
    EXPECT_FALSE(findDrawn(kBarColor, frame).has_value());

    const float top = getBounds(*gui, "content").y;
    wheel(frame.getCenter(), -2.0F);
    frames(30);
    const float wheeled = getBounds(*gui, "content").y;
    EXPECT_LT(wheeled, top - getUi().getTheme().getMetric(haylen::ui::Theme::Metric::ScrollLine));

    swipe(frame.getCenter(), frame.getCenter() - haylen::math::Vec2{0.0F, 200.0F});
    EXPECT_LT(getBounds(*gui, "content").y, wheeled - 150.0F);
}

// A dialog whose content fits the screen fades none of it at any spacing of its theme, whose fractions its sizes add up and subtract in different orders.
TEST_F(ScrollbarTest, DrawsNoFadeInTheBodyOfADialogThatFits) {
    for (int tenths = 100; tenths < 160; ++tenths) {
        const std::string spacing = std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
        SCOPED_TRACE(spacing);
        // clang-format off
        auto gui = mount(R"({"kind": "column", "style": {"metrics": {"itemSpacing": )" + spacing + R"(, "panelPadding": )" + spacing + R"(}, "fonts": {"body": {"size": 21.3}, "heading": {"size": 27.7}}}, "children": [
            {"kind": "dialog", "id": "safety", "title": "Safety", "message": "Share where you are with the people you trust, or call for help at once.", "buttons": [{"id": "close", "text": "Close"}, {"id": "help", "text": "Call for help"}]}
        ]})");
        // clang-format on
        frames(2);
        EXPECT_TRUE(getUi().getBackend().getFades().empty());
        getUi().unmount(*gui);
        frames(2);
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
    for (const Display& screen : kScreens) {
        SCOPED_TRACE(screen.name);
        use(screen);

        for (const char* name : {"Tool", "Inspector"}) {
            SCOPED_TRACE(name);
            ImGuiWindow* window = ImGui::FindWindowByName(name);
            ASSERT_NE(window, nullptr);

            if (std::string(name) == "Inspector") {
                ASSERT_FALSE(window->DC.ChildWindows.empty());
                window = window->DC.ChildWindows[0];
            }

            const haylen::math::Rect area{window->Pos.x, window->Pos.y, window->Size.x, window->Size.y};
            const haylen::math::Rect content = haylen::math::Rect::fromMinMax({window->WorkRect.Min.x, area.y}, {window->WorkRect.Max.x, area.getBottom()});
            expectApart(content, findBar(area), area, Side::End);
        }
    }
}

// The bars of immediate windows run in a groove of the track color with a thumb no shorter than the thumbs of the components, the way the bars of the components draw.
TEST_F(ScrollbarTest, DrawsTheBarsOfImmediateWindowsInAGroove) {
    // clang-format off
    getFixture().runLua(R"(
        local imgui = require('haylen.imgui')
        local scene = require('haylen.scene')
        local ui = require('haylen.ui')
        ui.setTheme(ui.addTheme({name = 'marked', colors = {scrollbar = '#FF00FF', scrollbarHover = '#FF00FF', scrollbarTrack = '#00FF00'}}, 'dark'))
        scene.push({renderUi = function()
            if imgui.beginWindow('Log', {x = 20, y = 20, width = 300, height = 200}) then
                for index = 1, 200 do
                    imgui.text('Line ' .. index)
                end
            end
            imgui.endWindow()
        end})
    )");
    // clang-format on
    frames(3);
    const ImGuiWindow* window = ImGui::FindWindowByName("Log");
    ASSERT_NE(window, nullptr);
    const haylen::math::Rect area{window->Pos.x, window->Pos.y, window->Size.x, window->Size.y};
    const std::optional<haylen::math::Rect> groove = findDrawn(kGrooveColor, area);
    ASSERT_TRUE(groove.has_value());
    const haylen::math::Rect thumb = findBar(area);
    EXPECT_TRUE(groove->contains(thumb));
    EXPECT_NEAR(groove->width, thumb.width, 0.01F);
    EXPECT_GE(thumb.height, haylen::ui::Scrollbar::kMinThumb);
}

TEST_F(ScrollbarTest, KeepsTheGapForBarsDrawnWithThemeImages) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "style": {"surfaces": {"scrollbar": {"image": "ui/thumb.png", "slice": 4}, "scrollbarTrack": {"image": "ui/groove.png", "slice": 4}}}, "children": [
        {"kind": "scroll", "id": "area", "scrollbar": true, "width": 300, "height": 240, "children": [{"kind": "column", "id": "content", "children": [)" + labels(30) + R"(]}]}
    ]})");
    // clang-format on
    // The renderer names the texture of a draw on its first quad, so the quads after it belong to the same image.
    std::vector<haylen::math::Rect> thumbs;
    std::vector<haylen::math::Rect> grooves;
    std::vector<haylen::math::Rect>* image = nullptr;
    haylen::graphics2d::Renderer& renderer = getEngine().getRenderer2D();
    // clang-format off
    const std::uint64_t overlay = renderer.addCanvasOverlay([&](haylen::graphics2d::Renderer& drawing) {
        drawing.visitDrawn([&](const haylen::graphics2d::Renderer::Drawn& drawn) {
            if (drawn.text || !drawn.label.empty()) {
                image = drawn.label == "ui/thumb.png" ? &thumbs : drawn.label == "ui/groove.png" ? &grooves : nullptr;
            }

            if (image != nullptr) {
                image->push_back(haylen::math::Geometry2D::bounds(drawn.corners));
            }
        });
    });
    // clang-format on

    // The images of the theme load on the worker threads, so the frames go on until the bars draw with them, and then one more frame draws them in place.
    ASSERT_TRUE(getFixture().frameUntil([&] { return !thumbs.empty() && !grooves.empty(); }));
    thumbs.clear();
    grooves.clear();
    frames();
    renderer.removeCanvasOverlay(overlay);
    ASSERT_FALSE(thumbs.empty());
    ASSERT_FALSE(grooves.empty());

    const haylen::math::Rect area = getBounds(*gui, "area");
    expectApart(getBounds(*gui, "content"), merge(grooves), area, Side::End);
    EXPECT_TRUE(merge(grooves).contains(merge(thumbs)));
}

} // namespace haylen::lua::ui
