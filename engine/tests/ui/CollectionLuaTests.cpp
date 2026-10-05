#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/platform/Event.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::ui {

namespace {

class CollectionLuaTest : public ::testing::Test {
  protected:
    // Mounts `gui` with the collection `list` of rows 60 units tall at the top of the screen, ten of which show, and gives it `count` items in the global `items`.
    void mountRows(int count, const std::string& more = {}) {
        fixture.runLua("ui = require('haylen.ui') items = {} for i = 1, " + std::to_string(count) + " do items[i] = {id = 'i' .. i, title = 'Item ' .. i} end");
        fixture.runLua("gui = ui.mount(ui.column{ui.collection{id = 'list', height = 600, gap = 0, prefetch = 0" + more + ", types = {row = {template = ui.label{part = 'title', bind = {text = 'title'}, height = 60}}}}}, {placement = 'screen'}) list = gui:collection('list') list:setItems(items)");
        fixture.frames(3);
    }

    // Clicks the middle of a row of the list, from 1.
    void clickRow(int row, float x = 100.0F) {
        const std::string top = fixture.lua("return gui:bounds('list').y");
        const std::string left = fixture.lua("return gui:bounds('list').x");
        const math::Vec2 origin = getEngine().getViewport().getVisibleRect().getMin();
        const math::Vec2 point = math::Vec2{std::stof(left) + x, std::stof(top) + 60.0F * static_cast<float>(row) - 30.0F} - origin;
        for (const platform::Event::Type type : {platform::Event::Type::MouseMove, platform::Event::Type::MouseDown, platform::Event::Type::MouseUp}) {
            platform::Event event;
            event.type = type;
            event.position = getEngine().getViewport().toFramebuffer(point + origin);
            getEngine().handleEvent(event);
            fixture.frames(1);
        }
        fixture.frames(1);
    }

    core::Engine& getEngine() {
        return fixture.engine();
    }

    // Runs frames until the app fails and returns the message, which stops the app.
    std::string waitForError() {
        if (!fixture.frameUntil([&] { return getEngine().getError() != nullptr; }, std::chrono::seconds(5))) {
            return "no error";
        }
        return getEngine().getError()->what();
    }

    test::EngineFixture fixture;
};

} // namespace

TEST_F(CollectionLuaTest, ChangesTheListAndTheCollectionTogether) {
    mountRows(100);
    EXPECT_EQ(fixture.lua("return list.count .. ' ' .. tostring(gui:collection('list') == list)"), "100 true");
    fixture.runLua("list:insert(1, {{id = 'new', title = 'New'}, {id = 'newer', title = 'Newer'}})");
    EXPECT_EQ(fixture.lua("return list.count .. ' ' .. items[1].id .. ' ' .. items[3].id .. ' ' .. #items .. ' ' .. list:indexOf('i1')"), "102 new i1 102 3");
    fixture.runLua("list:remove(1, 2)");
    EXPECT_EQ(fixture.lua("return list.count .. ' ' .. items[1].id .. ' ' .. tostring(items[101]) .. ' ' .. tostring(list:indexOf('new'))"), "100 i1 nil nil");
    fixture.runLua("list:move(1, 3)");
    EXPECT_EQ(fixture.lua("return items[1].id .. items[2].id .. items[3].id .. list:indexOf('i1')"), "i2i3i13");
    fixture.runLua("list:replace(2, {id = 'other', title = 'Other'})");
    EXPECT_EQ(fixture.lua("return items[2].id .. ' ' .. tostring(list:indexOf('i3'))"), "other nil");
    fixture.runLua("items[5].title = 'Changed' list:reload('i6') list:reload({1, 'i7'})");
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("local first, last = list:visibleRange() return first .. ' ' .. last"), "1 10");

    // A second list compares with the first by id, and the same list changed in place too.
    fixture.runLua("table.insert(items, 1, {id = 'head', title = 'Head'}) list:setItems(items)");
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return list.count .. ' ' .. list:indexOf('head')"), "101 1");
    EXPECT_EQ(getEngine().getError(), nullptr);
}

TEST_F(CollectionLuaTest, ReportsTheErrorsOfItsMethods) {
    mountRows(100);
    const auto error = [this](const std::string& source) { return fixture.lua(source); };
    EXPECT_NE(error("gui:collection('missing')").find("The GUI has no node with the id \"missing\"."), std::string::npos);
    EXPECT_NE(error("local other = ui.mount(ui.label{id = 'title', text = 'x'}) other:collection('title')").find("The node \"title\" is a \"label\", not a \"collection\"."), std::string::npos);
    EXPECT_NE(error("list:setItems({{id = 'a'}, {title = 'b'}})").find("The item at index 2 of the collection \"list\" needs a non-empty string id."), std::string::npos);
    EXPECT_NE(error("list:setItems({{id = 'a'}, 3})").find("The item at index 2 of the collection \"list\" must be a table."), std::string::npos);
    EXPECT_NE(error("list:setItems({{id = 'a'}, {id = 'a'}})").find("The items of the collection \"list\" use the id \"a\" more than once."), std::string::npos);
    EXPECT_NE(error("list:setItems({{id = 'a', type = 'card'}})").find("The item \"a\" of the collection \"list\" has the type \"card\", which the collection does not declare."), std::string::npos);
    EXPECT_NE(error("list:setItems({{id = 'a', type = 3}})").find("The type of the item \"a\" of the collection \"list\" must be a string."), std::string::npos);
    EXPECT_NE(error("list:insert(1, {{id = 'i5'}})").find("The items of the collection \"list\" use the id \"i5\" more than once."), std::string::npos);
    EXPECT_NE(error("list:scrollTo('zzz')").find("The collection \"list\" has no item with the id \"zzz\"."), std::string::npos);
    EXPECT_NE(error("list:remove(500)").find("The index 500 is outside the items of the collection \"list\", which holds 100."), std::string::npos);
    EXPECT_NE(error("list:insert(102, {})").find("The index 102 is outside the items of the collection \"list\", which holds 100."), std::string::npos);
    EXPECT_NE(error("list:scrollTo(1, {speed = 2})").find("Unknown option \"speed\"."), std::string::npos);
    EXPECT_NE(error("list:scrollTo(1, {align = 'top'})").find("The option \"align\" must be \"nearest\", \"start\", \"center\" or \"end\"."), std::string::npos);
    EXPECT_NE(error("list:setBinder('card', function() end)").find("The collection \"list\" has no type named \"card\"."), std::string::npos);
    EXPECT_NE(error("return list.nothing").find("The type \"haylen.UiCollection\" has no member \"nothing\"."), std::string::npos);
    EXPECT_NE(error("list.count = 3").find("The type \"haylen.UiCollection\" has no writable property \"count\"."), std::string::npos);
    EXPECT_NE(error("list:setPages({count = 10})").find("The option \"load\" must be a function."), std::string::npos);
    EXPECT_NE(error("list:setPages({load = function() end})").find("The option \"count\" is required."), std::string::npos);
    EXPECT_NE(error("list:setPages({count = 10, pageSize = 0, load = function() end})").find("The option \"pageSize\" must be at least 1."), std::string::npos);
    EXPECT_NE(error("list:restoreState({item = 'i1', place = 3})").find("Unknown option \"place\"."), std::string::npos);

    fixture.runLua("multi = ui.mount(ui.collection{id = 'multi', height = 100, types = {a = {template = ui.label{}}, b = {template = ui.label{}}}}):collection('multi')");
    EXPECT_NE(error("multi:setItems({{id = 'x'}})").find("The item \"x\" of the collection \"multi\" needs a type, because the collection declares several."), std::string::npos);
    EXPECT_NE(error("multi:insert(1, {})").find("The collection \"multi\" has no list of items. Give it one with \"setItems\"."), std::string::npos);
    fixture.runLua("multi:setPages({count = 10, load = function(first, count) local page = {} for i = 1, count do page[i] = {id = 'p' .. (first + i - 1), type = 'a'} end return page end})");
    EXPECT_NE(error("multi:remove(1)").find("The collection \"multi\" pages its items, so it changes them with \"setPages\"."), std::string::npos);

    // Templates name their nodes with parts and take no handlers.
    EXPECT_NE(error("ui.mount(ui.collection{types = {row = {template = ui.button{text = 'Buy', onClick = function() end}}}})").find("A collection template takes no handlers. Handle the events of its parts on the collection, where they arrive with \"part\" and \"cell\"."), std::string::npos);
    EXPECT_NE(error("ui.mount(ui.collection{types = {row = {template = ui.label{id = 'name'}}}})").find("A node inside a collection template names itself with \"part\", not \"id\"."), std::string::npos);
    EXPECT_NE(error("gui:set('list', {types = {row = {template = ui.row{ui.label{part = 'a'}, ui.label{part = 'a'}}}}})").find("The part \"a\" is used more than once in the template of the type \"row\"."), std::string::npos);

    fixture.runLua("gui:unmount()");
    EXPECT_NE(error("return list.count").find("The GUI is not mounted."), std::string::npos);
    EXPECT_EQ(getEngine().getError(), nullptr);
}

TEST_F(CollectionLuaTest, ReadsItsStateThroughProperties) {
    mountRows(100, ", selection = 'multiple', selected = {'i3', 'i1'}");
    EXPECT_EQ(fixture.lua("return list.contentLength .. ' ' .. list.viewportLength .. ' ' .. list.scrollOffset .. ' ' .. table.concat(list.selected, ',')"), "6000.0 600.0 0.0 i1,i3");
    fixture.runLua("list.scrollOffset = 120");
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("local first = list:visibleRange() return list.scrollOffset .. ' ' .. first"), "120.0 3");
    fixture.runLua("list:scrollBy(60, {animated = false}) list:focus('i7')");
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return list.scrollOffset .. ' ' .. list.focusedItem"), "180.0 i7");
    EXPECT_EQ(fixture.lua("local gui, id, item, part = ui.focused() return id .. ' ' .. item .. ' ' .. tostring(part)"), "list i7 nil");

    fixture.runLua("state = list:saveState() list.scrollOffset = 3000 list:restoreState(state)");
    fixture.frames(4);
    EXPECT_EQ(fixture.lua("return state.item .. ' ' .. state.distance .. ' ' .. state.focused .. ' ' .. list.scrollOffset"), "i4 0.0 i7 180.0");
}

TEST_F(CollectionLuaTest, CallsTheLuaBinderOncePerBoundCell) {
    fixture.runLua("binds = 0");
    mountRows(1000);
    fixture.runLua("list:setBinder('row', function(cell, item, index) binds = binds + 1 cell:set('title', {text = item.title .. ' at ' .. index}) end)");
    fixture.frames(30);
    // Ten rows show and one more binds on each side that has items.
    EXPECT_EQ(fixture.lua("return binds"), "11");
    fixture.frames(60);
    EXPECT_EQ(fixture.lua("return binds"), "11");
    fixture.runLua("list:scrollBy(60, {animated = false})");
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return binds"), "12");
    fixture.runLua("list:setBinder('row', nil)");
    fixture.runLua("list:scrollBy(60, {animated = false})");
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return binds"), "12");
    EXPECT_EQ(getEngine().getError(), nullptr);
}

TEST_F(CollectionLuaTest, RunsNoLuaWhileIdle) {
    // clang-format off
    fixture.runLua(R"(
        ui = require('haylen.ui')
        binds, loads, events = 0, 0, 0
        gui = ui.mount(ui.column{ui.collection{id = 'pages', height = 600, placeholder = 'wait', types = {
            entry = {template = ui.label{part = 'title', bind = {text = 'title'}, height = 60}},
            wait = {template = ui.busyIndicator{}, estimatedSize = 60},
        }}})
        pages = gui:collection('pages')
        pages:setBinder('entry', function() binds = binds + 1 end)
        pages:setPages({count = 100000, pageSize = 20, load = function(first, count)
            loads = loads + 1
            local page = {}
            for i = 1, count do page[i] = {id = 'e' .. (first + i - 1), type = 'entry', title = 'Entry ' .. (first + i - 1)} end
            return page
        end})
        ui.onEvent(function() events = events + 1 end)
    )");
    // clang-format on
    fixture.frames(30);
    const std::string settled = fixture.lua("return binds .. ' ' .. loads .. ' ' .. events");
    fixture.frames(120);
    EXPECT_EQ(fixture.lua("return binds .. ' ' .. loads .. ' ' .. events"), settled);
    EXPECT_EQ(getEngine().getError(), nullptr);
}

TEST_F(CollectionLuaTest, PagesItemsFromTheLuaLoader) {
    // clang-format off
    fixture.runLua(R"(
        ui = require('haylen.ui')
        loads = {}
        loader = function(first, count)
            loads[#loads + 1] = first
            local page = {}
            for i = 1, count do page[i] = {id = 'e' .. (first + i - 1), title = 'Entry ' .. (first + i - 1)} end
            return page
        end
        gui = ui.mount(ui.column{ui.collection{id = 'pages', height = 600, gap = 0, prefetch = 0, types = {entry = {template = ui.label{part = 'title', bind = {text = 'title'}, height = 60}}}}})
        pages = gui:collection('pages')
        pages:setPages({count = 100000, pageSize = 50, maxPages = 4, load = loader})
        loadsOf = function(first)
            local found = 0
            for _, loaded in ipairs(loads) do
                if loaded == first then found = found + 1 end
            end
            return found
        end
    )");
    // clang-format on
    fixture.frames(5);
    EXPECT_EQ(fixture.lua("return table.concat(loads, ',') .. ' ' .. pages.count .. ' ' .. pages:cellOf(1).item"), "1 100000 e1");

    // A jump loads the pages around the view only, and the pages kept within the limit do not load again.
    fixture.runLua("pages:scrollTo(50000, {animated = false})");
    fixture.frames(10);
    EXPECT_EQ(fixture.lua("return pages:cellOf(50000).item .. ' ' .. loadsOf(1) .. ' ' .. loadsOf(49951) .. ' ' .. tostring(#loads <= 4)"), "e50000 1 1 true");
    fixture.runLua("pages:scrollTo(1, {animated = false})");
    fixture.frames(10);
    EXPECT_EQ(fixture.lua("return pages:cellOf(1).item .. ' ' .. loadsOf(1)"), "e1 1");

    // The same loader keeps the pages it loaded while the count changes.
    const std::string before = fixture.lua("return #loads");
    fixture.runLua("pages:setPages({count = 200, pageSize = 50, maxPages = 4, load = loader})");
    fixture.frames(5);
    EXPECT_EQ(fixture.lua("return #loads .. ' ' .. pages.count .. ' ' .. pages:cellOf(1).item"), before + " 200 e1");
    EXPECT_EQ(getEngine().getError(), nullptr);
}

TEST_F(CollectionLuaTest, ShowsPlaceholdersUntilAPromiseResolves) {
    // clang-format off
    fixture.runLua(R"(
        ui = require('haylen.ui')
        jobs = require('haylen.jobs')
        gui = ui.mount(ui.column{ui.collection{id = 'pages', height = 600, placeholder = 'wait', types = {
            entry = {template = ui.label{part = 'title', bind = {text = 'title'}, height = 60}},
            wait = {template = ui.label{part = 'state', text = 'Loading', height = 60}},
        }}})
        pages = gui:collection('pages')
        pages:setPages({count = 1000, pageSize = 25, load = function(first, count)
            return jobs.spawn(function()
                local page = {}
                for i = 1, count do page[i] = {id = 'e' .. (first + i - 1), type = 'entry', title = 'Entry ' .. (first + i - 1)} end
                return page
            end)
        end})
    )");
    // clang-format on
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return pages:cellOf(1).type"), "wait");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("local cell = pages:cellOf(1) return cell and cell.type") == "entry"; }));
    EXPECT_EQ(fixture.lua("return pages:cellOf(1).item .. ' ' .. pages:indexOf('e2')"), "e1 2");
    EXPECT_EQ(getEngine().getError(), nullptr);
}

TEST_F(CollectionLuaTest, HandsBindersCellsThatGoStaleWithTheirItem) {
    mountRows(1000, ", poolSize = 1");
    fixture.runLua("list:setBinder('row', function(cell, item, index) if index == 1 then first = cell end end)");
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return first.item .. ' ' .. first.index .. ' ' .. first.type .. ' ' .. tostring(first.bound)"), "i1 1 row true");
    EXPECT_NE(fixture.lua("first:set('nothing', {text = 'x'})").find("The template of the type \"row\" has no part named \"nothing\"."), std::string::npos);
    EXPECT_EQ(fixture.lua("first:set('title', {text = 'First'}) first:transform('title').opacity = 0.5 return 'ok'"), "ok");
    fixture.runLua("list.scrollOffset = 30000");
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return tostring(first.bound)"), "false");
    EXPECT_NE(fixture.lua("return first.item").find("This cell no longer shows the item \"i1\"."), std::string::npos);
    EXPECT_NE(fixture.lua("first:set('title', {text = 'x'})").find("This cell no longer shows the item \"i1\"."), std::string::npos);
}

TEST_F(CollectionLuaTest, WritesPlayerValuesBackBeforeTheHandlersRun) {
    // clang-format off
    fixture.runLua(R"(
        ui = require('haylen.ui')
        rows = {}
        for i = 1, 30 do rows[i] = {id = 'r' .. i, on = false} end
        seen = 'none'
        gui = ui.mount(ui.column{ui.collection{id = 'list', height = 600, gap = 0, types = {row = {template = ui.toggle{part = 'switch', bind = {checked = 'on'}, height = 60}}},
            onChange = function(event)
                seen = event.part .. ' ' .. event.cell.item .. ' ' .. event.cell.index .. ' ' .. tostring(rows[event.cell.index].on) .. ' ' .. event.id
            end,
        }}, {placement = 'screen'})
        list = gui:collection('list')
        list:setItems(rows)
    )");
    // clang-format on
    fixture.frames(3);
    clickRow(2, 30.0F);
    EXPECT_EQ(fixture.lua("return seen"), "switch r2 2 true list");
    EXPECT_EQ(getEngine().getError(), nullptr);
}

TEST_F(CollectionLuaTest, ReportsSelectionsWithTheItem) {
    mountRows(50, ", selection = 'single', onSelect = function(event) picked = event.item .. ' ' .. event.index .. ' ' .. tostring(event.selected) end");
    clickRow(3);
    EXPECT_EQ(fixture.lua("return picked .. ' ' .. table.concat(list.selected, ',')"), "i3 3 true i3");
}

TEST_F(CollectionLuaTest, RefusesToChangeItsItemsWhileItBindsCells) {
    mountRows(100);
    fixture.runLua("list:setBinder('row', function() list:insert(1, {{id = 'x'}}) end) list.scrollOffset = 3000");
    EXPECT_NE(waitForError().find("The collection \"list\" cannot change its items while it binds cells."), std::string::npos);
}

TEST_F(CollectionLuaTest, RefusesToReplaceNodesWhileItBindsCells) {
    mountRows(100);
    fixture.runLua("list:setBinder('row', function() gui:replaceChildren('list', {}) end) list.scrollOffset = 3000");
    EXPECT_NE(waitForError().find("The GUI cannot replace nodes or unmount while its collections bind cells."), std::string::npos);
}

TEST_F(CollectionLuaTest, RefusesToUnmountWhileItBindsCells) {
    mountRows(100);
    fixture.runLua("list:setBinder('row', function() gui:unmount() end) list.scrollOffset = 3000");
    EXPECT_NE(waitForError().find("The GUI cannot replace nodes or unmount while its collections bind cells."), std::string::npos);
}

TEST_F(CollectionLuaTest, StopsTheAppOnABinderError) {
    mountRows(100);
    fixture.runLua("list:setBinder('row', function() error('the binder broke') end) list.scrollOffset = 3000");
    EXPECT_NE(waitForError().find("the binder broke"), std::string::npos);
}

TEST_F(CollectionLuaTest, StopsTheAppOnAnItemValueAPartRejects) {
    mountRows(100);
    fixture.runLua("items[70].title = {} list:reload(70) list.scrollOffset = 3900");
    EXPECT_NE(waitForError().find("The item \"i70\" gives the part \"title\" a value it does not accept."), std::string::npos);
}

TEST_F(CollectionLuaTest, StopsTheAppOnAPageOfTheWrongLength) {
    fixture.runLua("ui = require('haylen.ui') pages = ui.mount(ui.collection{id = 'pages', height = 600, types = {entry = {template = ui.label{}}}}):collection('pages')");
    fixture.runLua("pages:setPages({count = 100, pageSize = 10, load = function() return {{id = 'only'}} end})");
    EXPECT_NE(waitForError().find("The page loader of the collection \"pages\" returned 1 items for a page of 10."), std::string::npos);
}

} // namespace haylen::ui
