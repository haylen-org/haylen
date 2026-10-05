#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/input/GamepadButton.hpp"
#include "haylen/input/Key.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/text/Direction.hpp"
#include "haylen/ui/Collection.hpp"
#include "haylen/ui/CollectionCell.hpp"
#include "haylen/ui/CollectionItems.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Theme.hpp"
#include "support/AllocationTracker.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

// A source that counts the fields its cells read, which tells how many items were bound.
class CountingSource final : public CollectionItems {
  public:
    [[nodiscard]] core::Json getValue(std::size_t index, std::string_view field) const override {
        ++reads;
        return CollectionItems::getValue(index, field);
    }

    mutable std::size_t reads = 0;
};

class CollectionTest : public ::testing::Test, public test::UiFixture {
  protected:
    // A list 600 units tall of rows 60 units tall, which shows ten rows.
    static constexpr const char* kRows = R"({"kind": "collection", "id": "list", "height": 600, "gap": 0, "prefetch": 0, "poolSize": 4, "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}, "estimatedSize": 60}}})";

    // Items with the ids `i<first>` and on and a title each, of one type or of none.
    [[nodiscard]] static std::vector<CollectionItems::Item> makeItems(std::size_t count, std::size_t first = 1, const std::string& type = {}) {
        std::vector<CollectionItems::Item> items(count);
        for (std::size_t index = 0; index < count; ++index) {
            const std::string id = "i" + std::to_string(first + index);
            items[index] = {.id = id, .type = type, .fields = {{"title", id}}};
        }
        return items;
    }

    std::shared_ptr<Gui> mountCollection(const std::string& collection) {
        return mount(R"({"kind": "column", "children": [)" + collection + "]}");
    }

    std::shared_ptr<CountingSource> show(Collection& collection, std::vector<CollectionItems::Item> items) {
        auto source = std::make_shared<CountingSource>();
        source->assign(std::move(items));
        collection.setSource(source);
        frames(3);
        return source;
    }

    [[nodiscard]] static debug::ObjectCounter::Snapshot getCells() {
        return *debug::ObjectCounter::find("UiCell");
    }

    [[nodiscard]] static const math::Rect& getItemBounds(const Collection& collection, std::size_t index) {
        const CollectionCell* cell = collection.findCell(index);
        if (cell == nullptr) {
            throw std::logic_error("The item " + std::to_string(index) + " has no cell.");
        }
        return cell->getRoot().getBounds();
    }

    // The width the cells of a collection that scrolls take, which leave the lane of the scroll bar.
    [[nodiscard]] float getCellsWidth(const Collection& collection) {
        const Theme& theme = getUi().getTheme();
        return collection.getBounds().width - theme.getMetric(Theme::Metric::ScrollbarGap) - theme.getMetric(Theme::Metric::ScrollbarSize) - theme.getMetric(Theme::Metric::ScrollbarInset);
    }

    // The first press of a player who could not see the ring only shows it, so tests that navigate show it with a direction that leads nowhere.
    void showRing() {
        key(input::Key::Left);
    }

    [[nodiscard]] std::size_t countEvents(std::string_view name) const {
        const std::vector<std::string> names = getEventNames();
        return static_cast<std::size_t>(std::ranges::count(names, "list:" + std::string(name)));
    }
};

} // namespace

TEST_F(CollectionTest, KeepsTheCellsBoundedWhileScrollingOneHundredThousandItems) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    const debug::ObjectCounter::Snapshot before = getCells();
    show(list, makeItems(100000));
    const math::Rect area = list.getBounds();
    ASSERT_FLOAT_EQ(area.height, 600.0F);

    // Ten rows show, one more line binds on each side, a line beyond them stays bound and the pool keeps four cells.
    std::uint64_t created = 0;
    for (int step = 0; step < 40; ++step) {
        wheel(area.getCenter(), -600.0F / 64.0F);
        frames();
        EXPECT_LE(getCells().alive - before.alive, 18U) << step;
        if (step == 4) {
            created = getCells().created - before.created;
        }
    }
    EXPECT_EQ(getCells().created - before.created, created);

    list.setScrollOffset(1.0e12);
    frames(3);
    const math::Rect last = getItemBounds(list, 99999);
    EXPECT_NEAR(last.getBottom(), area.getBottom(), 0.5F);
}

TEST_F(CollectionTest, RecyclesCellsOfTheRightType) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "gap": 0, "poolSize": 2, "types": {
        "header": {"template": {"kind": "sectionTitle", "part": "heading", "bind": {"text": "title"}}},
        "row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}}
    }})");
    Collection& list = gui->getCollection("list");
    std::vector<CollectionItems::Item> items = makeItems(1000, 1, "row");
    for (std::size_t index = 0; index < items.size(); index += 10) {
        items[index].type = "header";
    }
    show(list, items);
    for (int step = 0; step < 30; ++step) {
        wheel(list.getBounds().getCenter(), -5.0F);
        frames();
        const auto [first, last] = *list.getVisibleRange();
        for (std::size_t index = first; index <= last; ++index) {
            const CollectionCell* cell = list.findCell(index);
            ASSERT_NE(cell, nullptr);
            const bool header = index % 10 == 0;
            EXPECT_EQ(cell->getType(), header ? "header" : "row");
            EXPECT_NE(cell->findPart(header ? "heading" : "title"), nullptr);
            EXPECT_EQ(cell->getItem(), "i" + std::to_string(index + 1));
        }
    }
}

TEST_F(CollectionTest, BindsOnlyNewlyVisibleItems) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    const std::shared_ptr<CountingSource> source = show(list, makeItems(1000));
    frames(5);
    const std::size_t reads = source->reads;
    frames(30);
    EXPECT_EQ(source->reads, reads);

    list.scrollBy(60.0, false);
    frames(2);
    EXPECT_EQ(source->reads, reads + 1);
}

TEST_F(CollectionTest, KeepsTheAnchorWhenItemsAreInsertedAbove) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    const std::shared_ptr<CountingSource> source = show(list, makeItems(1000));
    list.setScrollOffset(500.0 * 60.0);
    frames(3);
    const float y = getItemBounds(list, 500).y;
    const double offset = list.getScrollOffset();

    source->insert(0, makeItems(20, 2000));
    frames(2);
    EXPECT_NEAR(getItemBounds(list, 520).y, y, 0.5F);
    EXPECT_NEAR(list.getScrollOffset(), offset + 20.0 * 60.0, 0.5);
}

TEST_F(CollectionTest, KeepsTheAnchorWhenItemsAboveAreMeasured) {
    // Rows are estimated at 40 units and measure 60, so every row measured above the view would push the rows in view down without the anchor.
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "gap": 0, "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}, "estimatedSize": 40}}})");
    Collection& list = gui->getCollection("list");
    show(list, makeItems(1000));
    list.scrollTo(300, {.align = Collection::ScrollAlign::Start, .offset = 0.0F, .animated = false});
    frames(6);
    ASSERT_NEAR(getItemBounds(list, 300).y, list.getBounds().y, 0.5F);

    for (int step = 0; step < 10; ++step) {
        const float before = getItemBounds(list, 300).y;
        list.scrollBy(-30.0, false);
        frames();
        EXPECT_NEAR(getItemBounds(list, 300).y, before + 30.0F, 0.5F) << step;
    }
}

TEST_F(CollectionTest, KeepsTheFocusOnTheItemWhenItemsAreInsertedAbove) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    const std::shared_ptr<CountingSource> source = show(list, makeItems(1000));
    list.focusItem(49);
    frames(40);
    ASSERT_EQ(getFocusedItem(), "i50");
    const float y = getItemBounds(list, 49).y;

    source->insert(0, makeItems(5, 2000));
    frames(2);
    EXPECT_EQ(getFocusedItem(), "i50");
    EXPECT_NEAR(getItemBounds(list, 54).y, y, 0.5F);
}

TEST_F(CollectionTest, KeepsTheFocusWhenTheFocusedItemScrollsOutOfView) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    show(list, makeItems(1000));
    const debug::ObjectCounter::Snapshot before = getCells();
    list.focusItem(2);
    frames(3);
    showRing();
    ASSERT_EQ(getFocusedItem(), "i3");

    // The wheel takes the item away, and the focus stays on it while no cell shows it.
    wheel(list.getBounds().getCenter(), -200.0F * 60.0F / 64.0F);
    frames(3);
    EXPECT_EQ(list.findCell(2), nullptr);
    EXPECT_EQ(getFocusedItem(), "i3");
    EXPECT_LE(getCells().alive, before.alive + 4U);

    // The first direction brings the item back into view, and the next one moves the focus.
    key(input::Key::Down);
    frames(40);
    EXPECT_EQ(getFocusedItem(), "i3");
    EXPECT_TRUE(list.getBounds().contains(getItemBounds(list, 2)));
    key(input::Key::Down);
    EXPECT_EQ(getFocusedItem(), "i4");
}

TEST_F(CollectionTest, MovesTheFocusPastTheVisibleItems) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    show(list, makeItems(1000));
    list.focusItem(0);
    frames(3);
    showRing();
    for (int step = 0; step < 40; ++step) {
        key(input::Key::Down);
    }
    frames(40);
    EXPECT_EQ(getFocusedItem(), "i41");
    EXPECT_TRUE(list.getBounds().contains(getItemBounds(list, 40)));
}

TEST_F(CollectionTest, ScrollsToItemsWithTheRequestedAlignment) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "gap": 0, "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}, "estimatedSize": 40}}})");
    Collection& list = gui->getCollection("list");
    show(list, makeItems(10000));
    const math::Rect area = list.getBounds();
    // clang-format off
    const auto scrollTo = [&](std::size_t index, Collection::ScrollAlign align) {
        list.scrollTo(index, {.align = align, .offset = 0.0F, .animated = false});
        frames(8);
        return getItemBounds(list, index);
    };
    // clang-format on

    EXPECT_NEAR(scrollTo(4999, Collection::ScrollAlign::Start).y, area.y, 0.5F);
    EXPECT_NEAR(scrollTo(4999, Collection::ScrollAlign::Center).getCenter().y, area.getCenter().y, 0.5F);
    EXPECT_NEAR(scrollTo(4999, Collection::ScrollAlign::End).getBottom(), area.getBottom(), 0.5F);

    (void)scrollTo(0, Collection::ScrollAlign::Start);
    EXPECT_NEAR(scrollTo(4999, Collection::ScrollAlign::Nearest).getBottom(), area.getBottom(), 0.5F);
    (void)scrollTo(9999, Collection::ScrollAlign::End);
    EXPECT_NEAR(scrollTo(4999, Collection::ScrollAlign::Nearest).y, area.y, 0.5F);

    // An item already in view stays where it is.
    const float shown = getItemBounds(list, 5002).y;
    EXPECT_NEAR(scrollTo(5002, Collection::ScrollAlign::Nearest).y, shown, 0.5F);
}

TEST_F(CollectionTest, AnimatesScrollsAndSettlesOnTheTarget) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    show(list, makeItems(1000));
    list.scrollTo(300, {.align = Collection::ScrollAlign::Start, .offset = 0.0F, .animated = true});
    frames(2);
    const double early = list.getScrollOffset();
    EXPECT_GT(early, 0.0);
    EXPECT_LT(early, 300.0 * 60.0);
    frames(60);
    EXPECT_NEAR(list.getScrollOffset(), 300.0 * 60.0, 0.5);
}

TEST_F(CollectionTest, PlacesGridSpansAndFullWidthHeaders) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "layout": "grid", "lanes": 3, "gap": 10, "types": {
        "header": {"template": {"kind": "label", "part": "heading", "bind": {"text": "title"}, "height": 40}, "span": "full"},
        "cell": {"template": {"kind": "label", "part": "title", "bind": {"text": "title", "height": "size"}}}
    }})");
    Collection& list = gui->getCollection("list");
    std::vector<CollectionItems::Item> items = makeItems(30, 1, "cell");
    for (std::size_t index = 0; index < items.size(); ++index) {
        items[index].fields["size"] = index == 2 ? 90 : 50;
        if (index % 7 == 0) {
            items[index].type = "header";
        }
    }
    show(list, items);
    const math::Rect area = list.getBounds();
    const float width = getCellsWidth(list);
    const float lane = (width - 20.0F) / 3.0F;

    EXPECT_FLOAT_EQ(getItemBounds(list, 0).width, width);
    EXPECT_FLOAT_EQ(getItemBounds(list, 0).x, area.x);
    for (const std::size_t index : {1U, 2U, 3U}) {
        EXPECT_NEAR(getItemBounds(list, index).width, lane, 1.0F);
        EXPECT_FLOAT_EQ(getItemBounds(list, index).height, 90.0F);
    }
    EXPECT_NEAR(getItemBounds(list, 2).x, area.x + (lane + 10.0F) * 1.0F, 1.0F);
    EXPECT_FLOAT_EQ(getItemBounds(list, 4).height, 50.0F);
    EXPECT_NEAR(getItemBounds(list, 4).y, getItemBounds(list, 1).getBottom() + 10.0F, 0.5F);
    EXPECT_FLOAT_EQ(getItemBounds(list, 7).width, width);
}

TEST_F(CollectionTest, PinsTheCurrentSectionHeader) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "gap": 0, "types": {
        "header": {"template": {"kind": "label", "part": "heading", "bind": {"text": "title"}, "height": 40}, "sticky": true, "estimatedSize": 40},
        "row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}, "estimatedSize": 60}
    }})");
    Collection& list = gui->getCollection("list");
    std::vector<CollectionItems::Item> items = makeItems(200, 1, "row");
    for (std::size_t index = 0; index < items.size(); index += 20) {
        items[index].type = "header";
    }
    show(list, items);
    const math::Rect area = list.getBounds();
    const double section = 40.0 + 19.0 * 60.0;

    list.setScrollOffset(section + 300.0);
    frames(3);
    EXPECT_FLOAT_EQ(getItemBounds(list, 20).y, area.y);

    list.setScrollOffset(section * 2.0 - 20.0);
    frames(3);
    EXPECT_NEAR(getItemBounds(list, 40).y, area.y + 20.0F, 0.5F);
    EXPECT_NEAR(getItemBounds(list, 20).getBottom(), getItemBounds(list, 40).y, 0.5F);
}

TEST_F(CollectionTest, KeepsSelectionAndFocusThroughAReorder) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "gap": 0, "selection": "multiple", "selected": ["i2", "i9"], "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}}}})");
    Collection& list = gui->getCollection("list");
    std::vector<CollectionItems::Item> items = makeItems(20);
    const std::shared_ptr<CountingSource> source = show(list, items);
    list.focusItem(8);
    frames(3);
    ASSERT_EQ(getFocusedItem(), "i9");

    std::ranges::reverse(items);
    source->assign(items);
    frames(3);
    std::set<std::string> chosen;
    for (const std::size_t index : list.getSelected()) {
        chosen.insert(std::string(source->getId(index)));
    }
    EXPECT_EQ(chosen, (std::set<std::string>{"i2", "i9"}));
    EXPECT_EQ(getFocusedItem(), "i9");
    EXPECT_EQ(list.getFocusedIndex(), 11U);
}

TEST_F(CollectionTest, SelectsItemsAsItsSelectionModeSays) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "gap": 0, "selection": "single", "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}}}})");
    Collection& list = gui->getCollection("list");
    show(list, makeItems(20));
    click(getItemBounds(list, 3).getCenter());
    click(getItemBounds(list, 5).getCenter());
    EXPECT_EQ(list.getSelected(), (std::vector<std::size_t>{5}));
    const Event& selected = findLastEvent("select");
    EXPECT_EQ(selected.id, "list");
    EXPECT_EQ(selected.value.at("item"), "i6");
    EXPECT_EQ(selected.value.at("index"), 6);
    EXPECT_EQ(selected.value.at("selected"), true);

    gui->set("list", core::Json::parse(R"({"selection": "multiple"})"));
    click(getItemBounds(list, 7).getCenter());
    click(getItemBounds(list, 5).getCenter());
    EXPECT_EQ(list.getSelected(), (std::vector<std::size_t>{7}));
    EXPECT_EQ(findLastEvent("select").value.at("selected"), false);
}

TEST_F(CollectionTest, ResetsPlayerChangedPartsWhenACellIsReused) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "gap": 0, "poolSize": 2, "types": {"row": {"template": {"kind": "toggle", "part": "switch", "bind": {"checked": "on"}, "height": 60}}}})");
    Collection& list = gui->getCollection("list");
    std::vector<CollectionItems::Item> items = makeItems(1000);
    for (CollectionItems::Item& item : items) {
        item.fields["on"] = false;
    }
    const std::shared_ptr<CountingSource> source = show(list, items);

    // The value the player changed goes back to the item before the handlers hear it.
    click(getItemBounds(list, 0).getCenter());
    const Event& changed = findLastEvent("change");
    EXPECT_EQ(changed.id, "list");
    EXPECT_EQ(changed.value.at("checked"), true);
    EXPECT_EQ(changed.value.at("part"), "switch");
    EXPECT_EQ(changed.value.at("cell").at("item"), "i1");
    EXPECT_EQ(source->getItem(0).fields.at("on"), true);

    // A cell that showed the first item shows another one with the switch off, so a press turns that one on.
    list.setScrollOffset(500.0 * 60.0);
    frames(3);
    const std::size_t shown = list.getVisibleRange()->first + 1;
    clearEvents();
    click(getItemBounds(list, shown).getCenter());
    EXPECT_EQ(findLastEvent("change").value.at("checked"), true);
    EXPECT_EQ(findLastEvent("change").value.at("cell").at("item"), "i" + std::to_string(shown + 1));

    list.setScrollOffset(0.0);
    frames(3);
    clearEvents();
    click(getItemBounds(list, 0).getCenter());
    EXPECT_EQ(findLastEvent("change").value.at("checked"), false);
}

TEST_F(CollectionTest, AnimatesMovesInsertionsAndRemovals) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    const std::shared_ptr<CountingSource> source = show(list, makeItems(20));
    const float top = list.getBounds().y;

    // A moved item slides from where it was to its new place.
    source->move(0, 5);
    frames(2);
    const float sliding = getItemBounds(list, 5).y;
    EXPECT_GT(sliding, top);
    EXPECT_LT(sliding, top + 300.0F);
    frames(40);
    EXPECT_FLOAT_EQ(getItemBounds(list, 5).y, top + 300.0F);

    // The cell of a removed item fades out where it was before it goes back to its pool, which keeps it for the next item.
    const std::uint64_t alive = getCells().alive;
    source->remove(2);
    frames(2);
    EXPECT_GE(getCells().alive, alive);
    frames(40);
    EXPECT_LE(getCells().alive, alive + 1U);

    // Without animations an item takes its new place at once.
    gui->set("list", core::Json::parse(R"({"animateChanges": false})"));
    source->move(0, 4);
    frames(2);
    EXPECT_FLOAT_EQ(getItemBounds(list, 4).y, top + 240.0F);
    EXPECT_FLOAT_EQ(getItemBounds(list, 0).y, top);
}

TEST_F(CollectionTest, ReportsPartEventsWithTheirCell) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "layout": "grid", "lanes": 3, "types": {
        "product": {"template": {"kind": "column", "children": [
            {"kind": "label", "part": "name", "bind": {"text": "title"}},
            {"kind": "button", "part": "buy", "text": "Buy"}
        ]}}
    }})");
    Collection& list = gui->getCollection("list");
    show(list, makeItems(30));
    const CollectionCell* cell = list.findCell(2);
    ASSERT_NE(cell, nullptr);
    click(cell->findPart("buy")->getBounds().getCenter());
    const Event& clicked = findLastEvent("click");
    EXPECT_EQ(clicked.id, "list");
    EXPECT_EQ(clicked.value.at("part"), "buy");
    EXPECT_EQ(clicked.value.at("cell").at("item"), "i3");
    EXPECT_EQ(clicked.value.at("cell").at("index"), 3);
    EXPECT_EQ(clicked.value.at("cell").at("type"), "product");
}

TEST_F(CollectionTest, ReportsEndReachedOncePerCount) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    const std::shared_ptr<CountingSource> source = show(list, makeItems(200));
    EXPECT_EQ(countEvents("startReached"), 1U);
    EXPECT_EQ(countEvents("endReached"), 0U);

    list.setScrollOffset(1.0e9);
    frames(30);
    EXPECT_EQ(countEvents("endReached"), 1U);
    EXPECT_EQ(findLastEvent("endReached").value.at("count"), 200);

    source->insert(200, makeItems(50, 1000));
    list.setScrollOffset(1.0e9);
    frames(5);
    EXPECT_EQ(countEvents("endReached"), 2U);
    EXPECT_EQ(findLastEvent("endReached").value.at("count"), 250);
}

TEST_F(CollectionTest, ReportsTheVisibleRangeAndTheEndOfAScroll) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    show(list, makeItems(1000));
    EXPECT_EQ(findLastEvent("visibleChange").value.at("first"), 1);
    EXPECT_EQ(findLastEvent("visibleChange").value.at("last"), 10);
    clearEvents();
    frames(10);
    EXPECT_TRUE(getEventNames().empty());

    list.scrollTo(99, {.align = Collection::ScrollAlign::Start, .offset = 0.0F, .animated = true});
    frames(60);
    EXPECT_EQ(findLastEvent("visibleChange").value.at("first"), 100);
    EXPECT_EQ(countEvents("scrollEnd"), 1U);
    EXPECT_EQ(findLastEvent("scrollEnd").value.at("item"), "i100");
}

TEST_F(CollectionTest, PullsToRefreshWithATouchDrag) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "gap": 0, "refreshable": true, "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}}}})");
    Collection& list = gui->getCollection("list");
    show(list, makeItems(100));
    const math::Vec2 start = list.getBounds().getCenter() - math::Vec2{0.0F, 200.0F};

    // A pull shorter than the refresh distance springs back without asking.
    touch(platform::Event::Type::TouchBegan, 1, start);
    for (int step = 1; step <= 4; ++step) {
        frames();
        touch(platform::Event::Type::TouchMoved, 1, start + math::Vec2{0.0F, 20.0F * static_cast<float>(step)});
    }
    frames();
    touch(platform::Event::Type::TouchEnded, 1, start + math::Vec2{0.0F, 80.0F});
    frames(60);
    EXPECT_EQ(countEvents("refresh"), 0U);
    EXPECT_DOUBLE_EQ(list.getScrollOffset(), 0.0);

    touch(platform::Event::Type::TouchBegan, 1, start);
    for (int step = 1; step <= 8; ++step) {
        frames();
        touch(platform::Event::Type::TouchMoved, 1, start + math::Vec2{0.0F, 50.0F * static_cast<float>(step)});
    }
    frames();
    touch(platform::Event::Type::TouchEnded, 1, start + math::Vec2{0.0F, 400.0F});
    frames(60);
    EXPECT_EQ(countEvents("refresh"), 1U);
    EXPECT_DOUBLE_EQ(list.getScrollOffset(), 0.0);
}

TEST_F(CollectionTest, FlingsWithATouchDragAlongItsAxis) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    show(list, makeItems(1000));
    const math::Vec2 start = list.getBounds().getCenter() + math::Vec2{0.0F, 200.0F};
    touch(platform::Event::Type::TouchBegan, 1, start);
    for (int step = 1; step <= 6; ++step) {
        frames();
        touch(platform::Event::Type::TouchMoved, 1, start - math::Vec2{0.0F, 60.0F * static_cast<float>(step)});
    }
    frames();
    const double released = list.getScrollOffset();
    EXPECT_GT(released, 200.0);
    touch(platform::Event::Type::TouchEnded, 1, start - math::Vec2{0.0F, 360.0F});
    frames(120);
    EXPECT_GT(list.getScrollOffset(), released + 300.0);
    EXPECT_EQ(countEvents("select"), 0U);
}

TEST_F(CollectionTest, MirrorsHorizontalCollectionsInRightToLeftLayouts) {
    getUi().setDirection(text::Direction::RightToLeft);
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "axis": "horizontal", "width": 1000, "height": 100, "gap": 0, "types": {"tile": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "width": 100}}}})");
    Collection& list = gui->getCollection("list");
    show(list, makeItems(100));
    const math::Rect area = list.getBounds();
    EXPECT_NEAR(getItemBounds(list, 0).getRight(), area.getRight(), 0.5F);
    EXPECT_NEAR(getItemBounds(list, 1).getRight(), getItemBounds(list, 0).x, 0.5F);

    const float before = getItemBounds(list, 0).x;
    wheel(area.getCenter(), -1.0F);
    frames();
    EXPECT_GT(list.getScrollOffset(), 0.0);
    EXPECT_GT(getItemBounds(list, 2).x, before - 200.0F);

    auto grid = mountCollection(R"({"kind": "collection", "id": "list", "height": 300, "layout": "grid", "lanes": 4, "gap": 0, "types": {"tile": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}}}})");
    Collection& tiles = grid->getCollection("list");
    show(tiles, makeItems(20));
    EXPECT_NEAR(getItemBounds(tiles, 0).getRight(), tiles.getBounds().getRight(), 0.5F);
    EXPECT_LT(getItemBounds(tiles, 1).x, getItemBounds(tiles, 0).x);
}

TEST_F(CollectionTest, WrapsTheFocusAroundTheEnds) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "gap": 0, "focusWrap": "vertical", "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}}}})");
    Collection& list = gui->getCollection("list");
    show(list, makeItems(500));
    list.focusItem(499);
    frames(60);
    showRing();
    ASSERT_EQ(getFocusedItem(), "i500");
    key(input::Key::Down);
    frames(60);
    EXPECT_EQ(getFocusedItem(), "i1");
    EXPECT_NEAR(list.getScrollOffset(), 0.0, 0.5);
    key(input::Key::Up);
    frames(60);
    EXPECT_EQ(getFocusedItem(), "i500");
}

TEST_F(CollectionTest, PagesWithPageKeysAndShoulderButtons) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    show(list, makeItems(1000));
    list.focusItem(0);
    frames(3);
    showRing();
    key(input::Key::PageDown);
    frames(40);
    EXPECT_EQ(getFocusedItem(), "i11");
    button(input::GamepadButton::RightShoulder);
    frames(40);
    EXPECT_EQ(getFocusedItem(), "i21");
    button(input::GamepadButton::LeftShoulder);
    frames(40);
    EXPECT_EQ(getFocusedItem(), "i11");
    key(input::Key::End);
    frames(60);
    EXPECT_EQ(getFocusedItem(), "i1000");
    key(input::Key::Home);
    frames(60);
    EXPECT_EQ(getFocusedItem(), "i1");
}

TEST_F(CollectionTest, MovesTheFocusWhenTheFocusedItemIsRemoved) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    const std::shared_ptr<CountingSource> source = show(list, makeItems(100));
    list.focusItem(19);
    frames(40);
    ASSERT_EQ(getFocusedItem(), "i20");
    source->remove(19);
    frames(3);
    EXPECT_EQ(getFocusedItem(), "i21");
}

TEST_F(CollectionTest, RestoresTheSavedStateById) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    show(list, makeItems(1000));
    list.setScrollOffset(400.0 * 60.0 + 25.0);
    frames(3);
    list.focusItem(404);
    frames(3);
    const Collection::State state = list.saveState();
    EXPECT_EQ(state.item, "i401");
    EXPECT_FLOAT_EQ(state.distance, -25.0F);
    EXPECT_EQ(state.focused, "i405");
    getUi().unmount(*gui);

    auto again = mountCollection(kRows);
    Collection& restored = again->getCollection("list");
    std::vector<CollectionItems::Item> items = makeItems(1000);
    std::ranges::reverse(items);
    show(restored, items);
    restored.restoreState(state);
    frames(8);
    EXPECT_NEAR(getItemBounds(restored, 599).y - restored.getBounds().y, -25.0F, 0.5F);
    EXPECT_EQ(getFocusedItem(), "i405");
}

TEST_F(CollectionTest, SnapsToTheNearestItemOnceItRests) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "gap": 0, "snap": "item", "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}}}})");
    Collection& list = gui->getCollection("list");
    show(list, makeItems(100));
    wheel(list.getBounds().getCenter(), -100.0F / 64.0F);
    frames(60);
    EXPECT_NEAR(list.getScrollOffset(), 120.0, 0.5);
}

TEST_F(CollectionTest, ScrollsWithTheThumbAndTheTrackOfItsScrollbar) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    show(list, makeItems(1000));
    const math::Rect area = list.getBounds();
    const double maximum = 1000.0 * 60.0 - 600.0;

    // The thumb starts at the top of the bar on the end edge, and dragging it to the middle shows the middle of the content.
    const math::Vec2 thumb{area.getRight() - 8.0F, area.y + 10.0F};
    drag(thumb, thumb + math::Vec2{0.0F, (600.0F - 32.0F) * 0.5F});
    EXPECT_NEAR(list.getScrollOffset(), maximum * 0.5, maximum * 0.01);

    // A press on the track below the thumb moves by one view.
    const double before = list.getScrollOffset();
    click({area.getRight() - 8.0F, area.getBottom() - 10.0F});
    frames(40);
    EXPECT_NEAR(list.getScrollOffset(), before + 600.0, 1.0);
}

TEST_F(CollectionTest, RemembersTheLastFocusedItemWhenTheFocusEntersAgain) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "button", "id": "top", "text": "Top"},
        {"kind": "collection", "id": "list", "height": 600, "gap": 0, "rememberFocus": true, "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}}}}]})");
    Collection& list = gui->getCollection("list");
    show(list, makeItems(100));
    list.focusItem(4);
    frames(3);
    showRing();
    ASSERT_EQ(getFocusedItem(), "i5");
    gui->command(getUi().getContext(), "top", "focus", core::Json::object());
    frames(2);
    ASSERT_TRUE(isFocused(*gui, "top"));
    key(input::Key::Down);
    EXPECT_EQ(getFocusedItem(), "i5");
}

TEST_F(CollectionTest, FocusesTheFirstItemInViewOnTheFocusCommand) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "button", "id": "top", "text": "Top"},
        {"kind": "collection", "id": "list", "height": 600, "gap": 0, "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}}}}]})");
    Collection& list = gui->getCollection("list");
    show(list, makeItems(100));
    list.scrollTo(10, {.align = Collection::ScrollAlign::Start, .offset = 0.0F, .animated = false});
    frames(3);
    showRing();
    gui->command(getUi().getContext(), "list", "focus", core::Json::object());
    frames(3);
    EXPECT_EQ(getFocusedItem(), "i11");
    EXPECT_THROW(gui->command(getUi().getContext(), "list", "focus", core::Json{{"item", 1}}), std::invalid_argument);
}

TEST_F(CollectionTest, KeepsTheFocusedItemInTheMiddleWithCenterAlignment) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "gap": 0, "focusAlign": "center", "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}}}})");
    Collection& list = gui->getCollection("list");
    show(list, makeItems(100));
    list.focusItem(20);
    frames(60);
    EXPECT_EQ(getFocusedItem(), "i21");
    EXPECT_NEAR(getItemBounds(list, 20).getCenter().y, list.getBounds().getCenter().y, 1.0F);
    showRing();
    key(input::Key::Down);
    frames(60);
    EXPECT_EQ(getFocusedItem(), "i22");
    EXPECT_NEAR(getItemBounds(list, 21).getCenter().y, list.getBounds().getCenter().y, 1.0F);
}

TEST_F(CollectionTest, SelectsTheFocusedItemWhenSelectionFollowsTheFocus) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "gap": 0, "selection": "single", "selectionFollowsFocus": true, "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}}}})");
    Collection& list = gui->getCollection("list");
    show(list, makeItems(100));
    list.focusItem(0);
    frames(3);
    showRing();
    clearEvents();
    key(input::Key::Down);
    EXPECT_EQ(findLastEvent("itemFocus").value.at("item"), "i2");
    EXPECT_EQ(findLastEvent("itemFocus").value.at("index"), 2);
    EXPECT_EQ(findLastEvent("select").value.at("item"), "i2");
    EXPECT_EQ(list.getSelected(), (std::vector<std::size_t>{1}));
}

TEST_F(CollectionTest, FitsGridLanesToTheMinimumCellSizeAndCellsToTheAspect) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 900, "layout": "grid", "minCellSize": 300, "cellAspect": 1.5, "gap": 20, "types": {"tile": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}}}}})");
    Collection& list = gui->getCollection("list");
    show(list, makeItems(40));
    const math::Rect area = list.getBounds();
    const float width = getCellsWidth(list);
    const auto lanes = static_cast<float>(static_cast<int>((width + 20.0F) / 320.0F));
    const float lane = (width - 20.0F * (lanes - 1.0F)) / lanes;
    EXPECT_NEAR(getItemBounds(list, 0).width, lane, 1.0F);
    EXPECT_NEAR(getItemBounds(list, 0).height, lane * 1.5F, 1.0F);
    EXPECT_NEAR(getItemBounds(list, static_cast<std::size_t>(lanes)).y, area.y + lane * 1.5F + 20.0F, 1.0F);
}

TEST_F(CollectionTest, SticksToTheEndAsItemsArrive) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "gap": 0, "stickToEnd": true, "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}, "height": 60}}}})");
    Collection& list = gui->getCollection("list");
    const std::shared_ptr<CountingSource> source = show(list, makeItems(3));
    const math::Rect area = list.getBounds();
    EXPECT_NEAR(getItemBounds(list, 2).getBottom(), area.getBottom(), 0.5F);

    source->insert(3, makeItems(30, 100));
    frames(3);
    list.setScrollOffset(1.0e9);
    frames(3);
    source->insert(33, makeItems(5, 200));
    frames(40);
    EXPECT_NEAR(getItemBounds(list, 37).getBottom(), area.getBottom(), 0.5F);
}

TEST_F(CollectionTest, ShowsItsChildWhileItHasNoItems) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "collection", "id": "list", "height": 600, "types": {"row": {"template": {"kind": "label", "part": "title", "bind": {"text": "title"}}}},
        "children": [{"kind": "emptyState", "id": "empty", "title": "Nothing here"}]}]})");
    EXPECT_FALSE(getBounds(*gui, "empty").isEmpty());
    Collection& list = gui->getCollection("list");
    show(list, makeItems(3));
    const math::Rect drawn = getBounds(*gui, "empty");
    frames(2);
    EXPECT_EQ(getBounds(*gui, "empty"), drawn);
    EXPECT_NE(list.findCell(0), nullptr);
}

TEST_F(CollectionTest, AllocatesNothingWhileIdle) {
    // The engine allocates a little every frame on its own, and the renderer whenever the UI draws, so the frames of a GUI with a collection of a hundred thousand cells that draw nothing must allocate exactly what the frames of the same GUI without it allocate.
    // clang-format off
    const auto measure = [this] {
        frames(10);
        const test::AllocationTracker tracker;
        frames(30);
        return tracker.getTotal();
    };
    // clang-format on
    auto plain = mount(R"({"kind": "column", "children": [{"kind": "spacer", "height": 600}]})");
    const std::size_t without = measure();
    getUi().unmount(*plain);

    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "gap": 0, "scrollbar": false, "types": {"row": {"template": {"kind": "spacer", "height": 60}}}})");
    show(gui->getCollection("list"), makeItems(100000));
    EXPECT_EQ(measure(), without);
}

TEST_F(CollectionTest, ReportsProblemsOfTypesAndTemplates) {
    // clang-format off
    const auto problem = [this](const std::string& collection) {
        try {
            (void)getUi().createGui(core::Json::parse(collection));
        } catch (const std::invalid_argument& error) {
            return std::string(error.what());
        }
        return std::string("no error");
    };
    // clang-format on
    EXPECT_EQ(problem(R"({"kind": "collection"})"), "A \"collection\" needs at least one type in \"types\".");
    EXPECT_EQ(problem(R"({"kind": "collection", "types": []})"), "The property \"types\" of a \"collection\" must map type names to type definitions.");
    EXPECT_EQ(problem(R"({"kind": "collection", "types": {"row": {"estimatedSize": 4}}})"), "The type \"row\" of a \"collection\" needs a \"template\" node.");
    EXPECT_EQ(problem(R"({"kind": "collection", "types": {"row": {"template": {"kind": "label"}, "size": 4}}})"), "Unknown key \"size\" in \"collection.types.row\".");
    EXPECT_EQ(problem(R"({"kind": "collection", "types": {"row": {"template": {"kind": "label", "id": "name"}}}})"), "A node inside a collection template names itself with \"part\", not \"id\".");
    EXPECT_EQ(problem(R"({"kind": "collection", "types": {"row": {"template": {"kind": "row", "children": [{"kind": "label", "part": "a"}, {"kind": "label", "part": "a"}]}}}})"), "The part \"a\" is used more than once in the template of the type \"row\".");
    EXPECT_EQ(problem(R"({"kind": "collection", "types": {"row": {"template": {"kind": "label", "width": -3}}}})"), "The template of the type \"row\" has a problem. The property \"width\" of a \"label\" must be a non-negative number or \"auto\".");
    EXPECT_EQ(problem(R"({"kind": "collection", "types": {"row": {"template": {"kind": "label", "bind": {"text": 3}}}}})"), "The \"bind\" of a \"label\" in the template of the type \"row\" must map property names to field names.");
    EXPECT_EQ(problem(R"({"kind": "collection", "types": {"row": {"template": {"kind": "scroll"}}}})"), "The template of the type \"row\" cannot hold a \"scroll\" or a \"collection\".");
    EXPECT_EQ(problem(R"({"kind": "collection", "placeholder": "wait", "types": {"row": {"template": {"kind": "label"}}}})"), "The property \"placeholder\" of a \"collection\" names the type \"wait\", which it does not declare.");
    EXPECT_EQ(problem(R"({"kind": "collection", "layout": "grid", "types": {"head": {"template": {"kind": "label"}, "sticky": true}}})"), "The sticky type \"head\" of a grid collection must span the whole line.");
    EXPECT_EQ(problem(R"({"kind": "collection", "types": {"row": {"template": {"kind": "label"}, "span": 0}}})"), "The \"span\" of the type \"row\" must be a whole number from 1 to 64 or \"full\".");
    EXPECT_EQ(problem(R"({"kind": "collection", "axis": "diagonal", "types": {"row": {"template": {"kind": "label"}}}})"), "The property \"axis\" of a \"collection\" must be \"vertical\" or \"horizontal\".");
}

TEST_F(CollectionTest, RejectsItemsOfUndeclaredTypes) {
    auto gui = mountCollection(R"({"kind": "collection", "id": "list", "height": 600, "types": {"a": {"template": {"kind": "label"}}, "b": {"template": {"kind": "label"}}}})");
    Collection& list = gui->getCollection("list");
    auto source = std::make_shared<CollectionItems>();
    source->assign({{.id = "x", .type = "c", .fields = core::Json::object()}});
    EXPECT_THROW(list.setSource(source), std::invalid_argument);
    try {
        list.checkType("y", "");
        FAIL() << "An item without a type was accepted.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The item \"y\" of the collection \"list\" needs a type, because the collection declares several.");
    }
    EXPECT_THROW(list.setBinder("c", {}), std::invalid_argument);
    EXPECT_THROW((void)gui->getCollection("missing"), std::invalid_argument);
}

TEST_F(CollectionTest, RunsTheBindersOfCppForNewlyBoundCellsOnly) {
    auto gui = mountCollection(kRows);
    Collection& list = gui->getCollection("list");
    std::size_t calls = 0;
    // clang-format off
    list.setBinder("row", [&calls](CollectionCell& cell, std::size_t index) {
        ++calls;
        cell.set("title", {{"text", "Row " + std::to_string(index + 1)}});
    });
    // clang-format on
    show(list, makeItems(1000));
    const std::size_t bound = calls;
    EXPECT_GE(bound, 11U);
    frames(20);
    EXPECT_EQ(calls, bound);
    EXPECT_THROW(list.findCell(0)->set("nothing", core::Json::object()), std::invalid_argument);

    list.scrollBy(60.0, false);
    frames(2);
    EXPECT_EQ(calls, bound + 1);
}

} // namespace haylen::ui
