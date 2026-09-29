#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/ScopedConnection.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::ui {

namespace {

class UiLuaTest : public ::testing::Test {
  protected:
    core::Engine& getEngine() {
        return fixture.engine();
    }

    void pointer(platform::Event::Type type, math::Vec2 point) {
        platform::Event event;
        event.type = type;
        event.position = getEngine().getViewport().toFramebuffer(point + getEngine().getViewport().getVisibleRect().getMin());
        getEngine().handleEvent(event);
    }

    // Clicks the center of the bounds Lua reports for a node, which are in design coordinates.
    void click(const std::string& document, const std::string& id) {
        const std::string center = fixture.lua("local b = " + document + ":bounds('" + id + "') return b.x + b.width / 2 .. ',' .. b.y + b.height / 2");
        const std::size_t comma = center.find(',');
        ASSERT_NE(comma, std::string::npos) << center;
        const math::Vec2 point = math::Vec2{std::stof(center.substr(0, comma)), std::stof(center.substr(comma + 1))} - getEngine().getViewport().getVisibleRect().getMin();
        pointer(platform::Event::Type::MouseMove, point);
        fixture.frames(1);
        pointer(platform::Event::Type::MouseDown, point);
        fixture.frames(1);
        pointer(platform::Event::Type::MouseUp, point);
        fixture.frames(2);
    }

    void key(input::Key code) {
        for (const platform::Event::Type type : {platform::Event::Type::KeyDown, platform::Event::Type::KeyUp}) {
            platform::Event event;
            event.type = type;
            event.key = code;
            getEngine().handleEvent(event);
            fixture.frames(1);
        }
    }

    test::EngineFixture fixture;
};

} // namespace

TEST_F(UiLuaTest, MountsTreesAndCallsHandlers) {
    // clang-format off
    fixture.runLua(R"(
        ui = require('haylen.ui')
        clicks = {}
        hud = ui.mount(ui.column{padding = 20,
            ui.label{id = 'day', text = 'Day 1'},
            ui.button{id = 'play', text = 'Play', onClick = function(event) clicks[#clicks + 1] = event.id .. ':' .. event.name .. ':' .. tostring(event.document == hud) end},
            ui.checkbox{text = 'Music', onChange = function(event) clicks[#clicks + 1] = 'music ' .. tostring(event.checked) .. ' ' .. event.id end},
        }, {placement = 'screen', layer = 2})
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(hud.mounted) .. ' ' .. hud.placement .. ' ' .. tostring(hud.visible) .. ' ' .. tostring(hud:has('play'))"), "true screen true true");
    EXPECT_EQ(fixture.lua("return hud:get('day').text .. ' ' .. tostring(hud:get('nothing'))"), "Day 1 nil");
    EXPECT_EQ(fixture.lua("return hud:bounds('play').height"), "64.0");

    click("hud", "play");
    click("hud", "#1");
    EXPECT_EQ(fixture.lua("return table.concat(clicks, ', ')"), "play:click:true, music true #1");

    // clang-format off
    fixture.runLua(R"(
        hud:set('day', {text = 'Day 2'})
        hud:set('play', {text = 'Go', onClick = function() clicks[#clicks + 1] = 'changed' end})
    )");
    // clang-format on
    fixture.frames(1);
    click("hud", "play");
    EXPECT_EQ(fixture.lua("return clicks[#clicks] .. ' ' .. hud:get('day').text .. ' ' .. hud:get('play').text"), "changed Day 2 Go");
}

TEST_F(UiLuaTest, PublishesMountsAndEndsWhatDocumentsOwn) {
    // clang-format off
    fixture.runLua(R"(
        ui = require('haylen.ui')
        events = require('haylen.events')
        timer = require('haylen.timer')
        heard = {}
        events.on('ui_document_mounted', function(document) mounted = document end)
        events.on('ui_document_unmounted', function(document) heard[#heard + 1] = 'unmounted ' .. tostring(document == hud) end)
        hud = ui.mount(ui.label{text = 'Day 1'})
        ticks = 0
        timer.every(0.01, function() ticks = ticks + 1 end, {owner = hud})
        events.on('score', function() heard[#heard + 1] = 'score' end, {owner = hud})
    )");
    // clang-format on
    fixture.frames(2);
    fixture.runLua("hud:unmount() events.emit('score')");
    const std::string ticks = fixture.lua("return ticks");
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return tostring(mounted == hud) .. ' ' .. table.concat(heard, ', ')"), "true unmounted true");
    EXPECT_EQ(fixture.lua("return ticks"), ticks);
    EXPECT_NE(ticks, "0");

    // Documents mounted from C++ reach C++ listeners with their pointer and Lua listeners with nil.
    plugins::UiPlugin& plugin = getEngine().getPlugin<plugins::UiPlugin>();
    std::shared_ptr<Document> received;
    const core::ScopedConnection listener = getEngine().getEvents().on(core::LifecycleEvent::kUiDocumentMounted, [&](core::EventBus::Event& event) { received = *event.get<std::shared_ptr<Document>>(); });
    const std::shared_ptr<Document> document = plugin.createDocument(core::Json::parse(R"({"kind": "label", "text": "native"})"));
    plugin.mount(document, 3);
    EXPECT_EQ(received, document);
    EXPECT_EQ(fixture.lua("return tostring(mounted)"), "nil");
    EXPECT_THROW(plugin.mount(document), std::invalid_argument);
    EXPECT_TRUE(plugin.unmount(*document));
    EXPECT_FALSE(plugin.unmount(*document));
}

TEST_F(UiLuaTest, MountsDeeplyNestedTrees) {
    fixture.runLua("ui = require('haylen.ui') function nested(levels) local node = ui.label{id = 'leaf', text = 'deep'} for level = 1, levels do node = ui.column{node} end return node end");
    fixture.runLua("deep = ui.mount(nested(62), {placement = 'screen'})");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return deep:get('leaf').text"), "deep");
    EXPECT_EQ(getEngine().getError(), nullptr);
    EXPECT_NE(fixture.lua("ui.mount(nested(1000))").find("limited to 64 levels"), std::string::npos);
}

TEST_F(UiLuaTest, ChangesHandlersOnlyAfterTheDocumentAcceptsTheChange) {
    // clang-format off
    fixture.runLua(R"(
        ui = require('haylen.ui')
        clicks = {}
        hud = ui.mount(ui.column{id = 'list', ui.button{id = 'play', text = 'Play', onClick = function() clicks[#clicks + 1] = 'old' end}}, {placement = 'screen'})
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_NE(fixture.lua("hud:set('play', {colour = 'red', onClick = function() clicks[#clicks + 1] = 'set' end})").find("button.colour is not a property"), std::string::npos);
    EXPECT_NE(fixture.lua("hud:replace('list', {ui.button{id = 'play', onClick = function() clicks[#clicks + 1] = 'replaced' end}, ui.label{id = 'list'}})").find("used more than once"), std::string::npos);
    EXPECT_NE(fixture.lua("hud:set('missing', {onClick = function() end})").find("no node with the id missing"), std::string::npos);
    click("hud", "play");
    EXPECT_EQ(fixture.lua("return table.concat(clicks, ',')"), "old");

    // A node built anew by a replace keeps none of the handlers of the node it replaced.
    fixture.runLua("hud:replace('list', {ui.button{id = 'play', text = 'Play'}})");
    fixture.frames(1);
    click("hud", "play");
    EXPECT_EQ(fixture.lua("return table.concat(clicks, ',')"), "old");

    fixture.runLua("hud:set('play', {onClick = function() clicks[#clicks + 1] = 'new' end})");
    click("hud", "play");
    EXPECT_EQ(fixture.lua("return tostring(hud:removeHandler('play', 'click')) .. ' ' .. tostring(hud:removeHandler('play', 'click'))"), "true false");
    click("hud", "play");
    EXPECT_EQ(fixture.lua("return table.concat(clicks, ',')"), "old,new");
    EXPECT_NE(fixture.lua("hud:removeHandler('missing', 'click')").find("no node with the id missing"), std::string::npos);
}

TEST_F(UiLuaTest, HandsEventValuesAndListenersToLua) {
    // clang-format off
    fixture.runLua(R"(
        ui = require('haylen.ui')
        seen = {}
        heard = {}
        listener = ui.onEvent(function(event) heard[#heard + 1] = event.id .. ':' .. event.name .. ':' .. tostring(event.document == menu) end)
        menu = ui.mount(ui.column{
            ui.list{id = 'slots', items = {{id = 'slot1', text = 'Slot 1'}}, onSelect = function(event) seen[#seen + 1] = event.id .. ' ' .. event.name .. ' ' .. event.item end},
            ui.dialog{id = 'quit', open = true, buttons = {{id = 'stay', text = 'Stay'}, {id = 'leave', text = 'Leave'}}, onAnswer = function(event) seen[#seen + 1] = event.id .. ' ' .. event.button end},
        }, {placement = 'screen'})
    )");
    // clang-format on
    fixture.frames(2);
    // The dialog focuses its last button without showing the ring, and the first Enter shows the ring and presses it.
    for (const platform::Event::Type type : {platform::Event::Type::KeyDown, platform::Event::Type::KeyUp}) {
        getEngine().handleEvent({.type = type, .key = input::Key::Enter});
        fixture.frames(1);
    }
    fixture.frames(2);
    click("menu", "slots");
    EXPECT_EQ(fixture.lua("return table.concat(seen, ',')"), "quit leave,slots select slot1");
    EXPECT_EQ(fixture.lua("return table.concat(heard, ',')"), "quit:focus:true,quit:answer:true,quit:blur:true,slots:focus:true,slots:select:true");

    fixture.runLua("listener:disconnect()");
    click("menu", "slots");
    EXPECT_EQ(fixture.lua("return #heard .. ' ' .. #seen"), "5 3");

    // Lua turns an empty table into an empty JSON object, and list properties read it as an empty list.
    EXPECT_EQ(fixture.lua("ui.mount(ui.column{ui.list{items = {}}, ui.tree{items = {}, expanded = {}}, ui.table{columns = {}, rows = {}}, ui.dialog{buttons = {}}}) return 'ok'"), "ok");
    EXPECT_NE(fixture.lua("listener = ui.onEvent('not a function')").find("error: "), std::string::npos);
}

TEST_F(UiLuaTest, ReplacesChildrenAndUnmounts) {
    // clang-format off
    fixture.runLua(R"(
        ui = require('haylen.ui')
        picked = {}
        menu = ui.mount(ui.node('column', {id = 'list', children = {ui.button{id = 'old', text = 'Old', onClick = function() end}}}), {placement = 'screen'})
        menu:replace('list', {ui.button{id = 'new', text = 'New', onClick = function(e) picked[#picked + 1] = e.id end}})
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(menu:has('old')) .. tostring(menu:has('new'))"), "falsetrue");
    click("menu", "new");
    EXPECT_EQ(fixture.lua("return table.concat(picked, ',')"), "new");

    EXPECT_NE(fixture.lua("menu:replace('list', {ui.label{id = 'list'}})").find("used more than once"), std::string::npos);
    EXPECT_EQ(fixture.lua("menu.visible = false return tostring(menu.visible)"), "false");
    EXPECT_EQ(fixture.lua("return tostring(menu:unmount()) .. tostring(menu:unmount()) .. tostring(menu.mounted)"), "truefalsefalse");
    EXPECT_NE(fixture.lua("menu:set('new', {text = 'x'})").find("not mounted"), std::string::npos);
}

TEST_F(UiLuaTest, ReportsMistakes) {
    fixture.runLua("ui = require('haylen.ui')");
    EXPECT_NE(fixture.lua("ui.node('spaceship')").find("no UI component kind named spaceship"), std::string::npos);
    EXPECT_NE(fixture.lua("return ui.spaceship").find("haylen.ui has no member 'spaceship'"), std::string::npos);
    EXPECT_NE(fixture.lua("ui.mount(ui.label{id = '#1'})").find("reserved"), std::string::npos);
    EXPECT_NE(fixture.lua("ui.mount(ui.column{ui.label{}, children = {}})").find("not both"), std::string::npos);
    EXPECT_NE(fixture.lua("ui.mount(ui.label{}, {placement = 'floor'})").find("safe or screen"), std::string::npos);
    EXPECT_NE(fixture.lua("ui.mount(ui.label{}, {depth = 1})").find("Unknown option 'depth'"), std::string::npos);
    EXPECT_NE(fixture.lua("ui.mount(ui.label{text = function() end})").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("ui.mount({kind = 'label', [true] = 1})").find("keys must be strings"), std::string::npos);

    // clang-format off
    fixture.runLua(R"(
        broken = ui.mount(ui.button{id = 'boom', text = 'Boom', onClick = function() error('handler failed') end}, {placement = 'screen'})
    )");
    // clang-format on
    fixture.frames(1);
    click("broken", "boom");
    ASSERT_NE(getEngine().getError(), nullptr);
    EXPECT_NE(std::string_view(getEngine().getError()->what()).find("handler failed"), std::string::npos);
}

TEST_F(UiLuaTest, PassesTheKeyboardOptionsOfTextComponents) {
    // clang-format off
    fixture.runLua(R"(
        ui = require('haylen.ui')
        form = ui.mount(ui.column{
            ui.textField{id = 'mail', keyboard = 'email', returnKey = 'next', autocorrect = false, autocapitalize = 'words', maxLength = 40},
            ui.textArea{id = 'notes', returnKey = 'send'},
            ui.numberField{id = 'age', decimals = 1, returnKey = 'done'},
        }, {placement = 'screen'})
    )");
    // clang-format on
    fixture.frames(1);
    using TextInput = platform::TextInput;
    const std::vector<TextInput::Field>& fields = fixture.host().getTextInput().getVisibleFields();
    ASSERT_EQ(fields.size(), 3U);
    EXPECT_EQ(fields[0].options, (TextInput::Options{.keyboard = TextInput::Keyboard::Email, .returnKey = TextInput::ReturnKey::Next, .capitalization = TextInput::Capitalization::Words, .autocorrect = false, .maxLength = 40}));
    EXPECT_EQ(fields[1].options, (TextInput::Options{.keyboard = TextInput::Keyboard::Multiline, .returnKey = TextInput::ReturnKey::Send}));
    EXPECT_EQ(fields[2].options, (TextInput::Options{.keyboard = TextInput::Keyboard::Decimal, .returnKey = TextInput::ReturnKey::Done, .capitalization = TextInput::Capitalization::None, .autocorrect = false}));

    fixture.runLua("form:set('mail', {keyboard = 'url'})");
    fixture.frames(1);
    EXPECT_EQ(fixture.host().getTextInput().getVisibleFields()[0].options.keyboard, TextInput::Keyboard::Url);
    EXPECT_NE(fixture.lua("ui.mount(ui.textField{keyboard = 'multiline'})").find("textField.keyboard"), std::string::npos) << "text areas and secret fields own those keyboards";
    EXPECT_NE(fixture.lua("ui.mount(ui.secretField{autocapitalize = 'shout'})").find("secretField.autocapitalize"), std::string::npos);
}

TEST_F(UiLuaTest, SwitchesThemesAndReadsInputCapture) {
    fixture.runLua("ui = require('haylen.ui')");
    EXPECT_EQ(fixture.lua("return ui.theme() .. ' ' .. table.concat(ui.themes(), ',')"), "dark dark,light");
    EXPECT_EQ(fixture.lua("ui.setTheme('light') return ui.theme()"), "light");
    EXPECT_NE(fixture.lua("ui.setTheme('marble')").find("no theme named marble"), std::string::npos);
    EXPECT_EQ(fixture.lua("return tostring(ui.wantsPointer()) .. tostring(ui.wantsKeyboard())"), "falsefalse");
    EXPECT_EQ(fixture.lua("local kinds = ui.kinds() return #kinds .. ' ' .. kinds[1]"), "62 accordion");
    EXPECT_NE(fixture.lua("ui.loadTheme('themes/none.json')").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("ui.addFont('pixel', 'fonts/none.ttf')").find("error: "), std::string::npos);
}

TEST(UiLuaThemeTest, ReadsAndAddsThemes) {
    const std::vector<std::uint8_t> panel = test::pngImage(24, 24, 0xFFFFFFFFU);
    test::EngineFixture fixture({{"content/ui/panel.png", std::string(panel.begin(), panel.end())}});
    fixture.runLua("ui = require('haylen.ui')");
    EXPECT_EQ(fixture.lua("return ui.themeColor('accent'):toHex() .. ' ' .. ui.themeMetric('controlHeight') .. ' ' .. ui.themeMetric('caretWidth')"), "#FF4C7DFF 64.0 2.0");
    EXPECT_EQ(fixture.lua("local font = ui.themeFont('title') return font.font .. ' ' .. font.size .. ' ' .. tostring(ui.themeSurface('panel'))"), "default 56.0 nil");

    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        return ui.addTheme({
            name = 'mint',
            colors = {accent = '#FF00AA88'},
            metrics = {controlHeight = 80},
            fonts = {title = {size = 72}},
            surfaces = {panel = {image = 'ui/panel.png', slice = 4, padding = {1, 2, 3, 4}, tint = '#FF808080', colorize = true}},
        }, 'light')
    )"), "mint");
    // clang-format on
    fixture.runLua("ui.setTheme('mint')");
    EXPECT_EQ(fixture.lua("return ui.themeColor('accent'):toHex() .. ' ' .. ui.themeColor('window'):toHex() .. ' ' .. ui.themeMetric('controlHeight') .. ' ' .. ui.themeFont('title').size"), "#FF00AA88 #FFF4F5F9 80.0 72.0");
    EXPECT_EQ(fixture.lua("local s = ui.themeSurface('panel') return tostring(s.slice):match('^haylen.NineSlice') .. ' ' .. s.scale .. ' ' .. table.concat(s.padding, ',') .. ' ' .. s.tint:toHex() .. ' ' .. tostring(s.colorize)"), "haylen.NineSlice 1.0 1.0,2.0,3.0,4.0 #FF808080 true");
    EXPECT_EQ(fixture.lua("return table.concat(ui.themes(), ',')"), "dark,light,mint");

    EXPECT_NE(fixture.lua("ui.themeColor('purple')").find("The theme has an unknown color role: purple"), std::string::npos);
    EXPECT_NE(fixture.lua("ui.themeMetric('height')").find("The theme has an unknown metric: height"), std::string::npos);
    EXPECT_NE(fixture.lua("ui.themeFont('huge')").find("The theme has an unknown font role: huge"), std::string::npos);
    EXPECT_NE(fixture.lua("ui.themeSurface('floor')").find("The theme has an unknown surface: floor"), std::string::npos);
    EXPECT_NE(fixture.lua("ui.addTheme({colors = {}})").find("A theme needs a name."), std::string::npos);
    EXPECT_NE(fixture.lua("ui.addTheme({name = 'x'}, 'marble')").find("no theme named marble to start from"), std::string::npos);
}

TEST_F(UiLuaTest, DrawsImmediateWindowsInsideScenes) {
    // clang-format off
    fixture.runLua(R"(
        imgui = require('haylen.imgui')
        scene = require('haylen.scene')
        results = {}
        scene.push({
            renderUi = function(self)
                local visible, open = imgui.beginWindow('Debug', {x = 10, y = 10, width = 500, height = 700, closable = true})
                if visible then
                    imgui.text('Frame')
                    imgui.textColored('#FFFF0000', 'Red')
                    imgui.textWrapped('Wrapped text')
                    results.button = imgui.button('Press', 120, 40)
                    results.check = select(2, imgui.checkbox('Check', true))
                    results.slider = select(2, imgui.sliderFloat('Slide', 0.5, 0, 1))
                    results.sliderInt = select(2, imgui.sliderInt('Count', 3, 0, 9))
                    results.drag = select(2, imgui.dragFloat('Drag', 2, 0.1, 0, 10))
                    results.text = select(2, imgui.inputText('Name', 'Ana', 'hint'))
                    results.float = select(2, imgui.inputFloat('Float', 1.5))
                    results.int = select(2, imgui.inputInt('Int', 7))
                    results.color = select(2, imgui.colorEdit('Color', '#FF336699')):toHex()
                    results.combo = select(2, imgui.combo('Combo', 2, {'a', 'b', 'c'}))
                    imgui.selectable('Pick', false)
                    if imgui.treeNode('Tree') then imgui.text('Inside') imgui.treePop() end
                    imgui.collapsingHeader('Header')
                    imgui.separator() imgui.sameLine() imgui.spacing() imgui.dummy(4, 4)
                    if imgui.beginChild('child', 200, 60, true) then imgui.text('Child') end
                    imgui.endChild()
                    if imgui.beginTabBar('tabs') then
                        if imgui.beginTabItem('Tab') then imgui.text('Tab body') imgui.endTabItem() end
                        imgui.endTabBar()
                    end
                    if imgui.beginTable('table', 2) then
                        imgui.tableSetupColumn('A') imgui.tableSetupColumn('B') imgui.tableHeadersRow()
                        imgui.tableNextRow() imgui.tableNextColumn() imgui.text('1')
                        imgui.endTable()
                    end
                    imgui.progressBar(0.5, 200, 20, 'half')
                    imgui.plotLines('Lines', {1, 3, 2}) imgui.plotHistogram('Bars', {1, 2, 3})
                    imgui.image(require('haylen.graphics').whiteTexture(), 16, 16)
                    imgui.setNextItemWidth(100) imgui.pushId('scope') imgui.popId()
                    imgui.pushFont('default', 40) imgui.text('Big') imgui.popFont()
                    results.hovered = imgui.isItemHovered()
                    imgui.openPopup('popup')
                    if imgui.beginPopup('popup') then imgui.text('Popup') imgui.closeCurrentPopup() imgui.endPopup() end
                    imgui.setTooltip('tip')
                end
                imgui.endWindow()
                imgui.setNextWindowPos(600, 10, true) imgui.setNextWindowSize(200, 100, true)
                imgui.beginWindow('Second') imgui.endWindow()
                results.open = open
                results.fps = imgui.framerate() >= 0
            end,
        })
    )");
    // clang-format on
    fixture.frames(3);
    ASSERT_EQ(getEngine().getError(), nullptr) << getEngine().getError()->what();
    EXPECT_EQ(fixture.lua("return tostring(results.button) .. ' ' .. tostring(results.check) .. ' ' .. results.slider .. ' ' .. results.sliderInt .. ' ' .. results.drag"), "false true 0.5 3 2.0");
    EXPECT_EQ(fixture.lua("return results.text .. ' ' .. results.float .. ' ' .. results.int .. ' ' .. results.color .. ' ' .. results.combo .. ' ' .. tostring(results.open) .. ' ' .. tostring(results.fps)"), "Ana 1.5 7 #FF336699 2 true true");
    EXPECT_NE(fixture.lua("require('haylen.imgui').text('late')").find("only be used while a frame is running"), std::string::npos);
}

TEST_F(UiLuaTest, AcceptsOnlyNumberFormatsInSliders) {
    // clang-format off
    fixture.runLua(R"(
        imgui = require('haylen.imgui')
        results = {}
        require('haylen.scene').push({renderUi = function()
            imgui.beginWindow('Formats')
            for _, format in ipairs({'%.0f%%', 'Volume %5.1f dB', '%lf', 'no number', '%s', '%d', '%.1f %.1f', '%*f', '%'}) do
                results[#results + 1] = tostring(pcall(imgui.sliderFloat, format, 0.5, 0, 1, format))
            end
            imgui.endWindow()
        end})
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(results, ' ')"), "true true true true false false false false false");
    EXPECT_EQ(getEngine().getError(), nullptr);
}

TEST_F(UiLuaTest, TurnsImGuiMisuseIntoScriptErrors) {
    // clang-format off
    fixture.runLua(R"(
        imgui = require('haylen.imgui')
        require('haylen.scene').push({renderUi = function() imgui.endWindow() end})
    )");
    // clang-format on
    fixture.frames(2);
    ASSERT_NE(getEngine().getError(), nullptr);
    EXPECT_NE(std::string_view(getEngine().getError()->what()).find("Dear ImGui check failed"), std::string::npos);
}

TEST_F(UiLuaTest, NavigatesTheFocusAndHearsCancel) {
    // clang-format off
    fixture.runLua(R"(
        ui = require('haylen.ui')
        heard = {}
        local function note(event) heard[#heard + 1] = event.name .. ' ' .. event.id end
        screen = ui.mount(ui.column{id = 'screen', onCancel = note,
            ui.button{id = 'play', text = 'Play', autofocus = true, onFocus = note, onBlur = note},
            ui.button{id = 'quit', text = 'Quit', focusUp = 'quit', onFocus = note},
        })
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("local document, id = ui.focused() return tostring(document == screen) .. ' ' .. id .. ' ' .. tostring(ui.focusRingVisible())"), "true play false");

    key(input::Key::Down);
    key(input::Key::Down);
    key(input::Key::Up);
    key(input::Key::Escape);
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("local _, id = ui.focused() return id .. ' ' .. tostring(ui.focusRingVisible())"), "quit true");
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "focus play, blur play, focus quit, cancel screen");

    fixture.runLua("ui.clearFocus()");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(ui.focused())"), "nil");
}

TEST_F(UiLuaTest, SimulatesTheSafeAreaAndKeepsTheBackButton) {
    // clang-format off
    fixture.runLua(R"(
        ui = require('haylen.ui')
        viewport = require('haylen.viewport')
        window = require('haylen.window')
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return tostring(window.backLeavesApp()) .. ' ' .. tostring(window.hasPointerDevice()) .. ' ' .. tostring(viewport.safeAreaSimulation())"), "true true nil");
    fixture.runLua("window.setBackLeavesApp(false) viewport.setSafeAreaSimulation('television') ui.setSafeAreaVisible(true)");
    fixture.frames(1);
    EXPECT_FALSE(getEngine().canBackLeaveApp());
    EXPECT_EQ(fixture.lua("local rect = viewport.safeRect() return viewport.safeAreaSimulation() .. ' ' .. rect.x .. ' ' .. rect.y .. ' ' .. tostring(ui.safeAreaVisible())"), "television 80.0 60.0 true");

    fixture.runLua("viewport.setSafeAreaSimulation({12, 0}) ui.setSafeAreaVisible(false)");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return viewport.safeRect().y .. ' ' .. table.concat(viewport.safeAreaSimulation(), ',')"), "12.0 12.0,0.0,12.0,0.0");
    EXPECT_EQ(fixture.lua("viewport.setSafeAreaSimulation(nil) return tostring(viewport.safeAreaSimulation()) .. ' ' .. viewport.safeRect().y"), "nil 0.0");
    EXPECT_NE(fixture.lua("viewport.setSafeAreaSimulation('watch')").find("There is no simulated device named watch."), std::string::npos);
}

TEST_F(UiLuaTest, BuildsTheComponentsForGamesAndApps) {
    // clang-format off
    fixture.runLua(R"(
        ui = require('haylen.ui')
        heard = {}
        local function note(event) heard[#heard + 1] = event.id .. ' ' .. event.name .. ' ' .. tostring(event.value or event.item or event.page or '') end
        screen = ui.mount(ui.column{padding = 20,
            ui.stepper{id = 'players', value = 1, min = 1, max = 3, autofocus = true, onChange = note},
            ui.segmentedControl{id = 'view', items = {{id = 'a', text = 'A'}, {id = 'b', text = 'B'}}, onChange = note},
            ui.rangeSlider{id = 'range', low = 0.2, high = 0.8},
            ui.circularProgress{id = 'cooldown', value = 0.5, style = 'cooldown'},
            ui.keyCapture{id = 'jump', value = 'key:space'},
            ui.accordion{id = 'faq', items = {{id = 'one', text = 'One'}}, ui.label{text = 'Answer'}},
            ui.carousel{id = 'pages', height = 120, ui.label{text = 'First'}, ui.label{text = 'Second'}},
            ui.slotGrid{id = 'bag', slots = {{id = 's1'}, {id = 's2'}}},
            ui.contextMenu{id = 'menu', items = {{id = 'copy', text = 'Copy'}}, onSelect = note, ui.label{text = 'Target'}},
            ui.window{id = 'map', title = 'Map', x = 900, y = 100, ui.label{text = 'Inside'}},
            ui.scroll{direction = 'horizontal', snap = true, width = 300, ui.row{ui.button{text = 'Card'}}},
        }, {placement = 'screen'})
    )");
    // clang-format on
    fixture.frames(2);
    EXPECT_EQ(getEngine().getError(), nullptr);
    EXPECT_EQ(fixture.lua("local names = {} for _, kind in ipairs(ui.kinds()) do names[kind] = true end return tostring(names.slotGrid and names.keyCapture and names.contextMenu and names.window and names.carousel)"), "true");

    key(input::Key::Right);
    key(input::Key::Right);
    key(input::Key::Down);
    key(input::Key::Right);
    key(input::Key::Right);
    fixture.runLua("screen:command('menu', 'open')");
    fixture.frames(2);
    key(input::Key::Enter);
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "players change 2.0, view change a, view change b, menu select copy");
    EXPECT_NE(fixture.lua("screen:set('players', {step = 0})").find("stepper.step must be greater than zero."), std::string::npos);
    EXPECT_NE(fixture.lua("screen:command('bag', 'open')").find("A slotGrid does not answer the command open."), std::string::npos);
}

} // namespace haylen::ui
