#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/platform/Accessibility.hpp"
#include "platform/AccessibilityRelay.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::ui {

using Element = haylen::platform::AccessibilityElement;

class AccessibilityTreeTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.host().setAccessibilityTreeWanted(true);
        fixture.runLua("ui = require('haylen.ui') accessibility = require('haylen.accessibility')");
    }

    // Returns the element of the last tree the platform received whose label is the text.
    [[nodiscard]] const Element* find(std::string_view label) {
        const std::vector<haylen::platform::HeadlessHost::AccessibilityTree>& trees = fixture.host().getAccessibilityTrees();
        if (trees.empty()) {
            return nullptr;
        }

        for (const Element& element : trees.back().elements) {
            if (element.label == label) {
                return &element;
            }
        }

        return nullptr;
    }

    // Returns the element of the last tree whose label is the text, which throws when there is none, so a test never reads an element that is missing.
    [[nodiscard]] const Element& get(std::string_view label) {
        const Element* element = find(label);
        if (element == nullptr) {
            throw std::runtime_error("The last accessibility tree has no element \"" + std::string(label) + "\".");
        }

        return *element;
    }

    [[nodiscard]] const haylen::platform::HeadlessHost::AccessibilityTree& getLastTree() {
        const std::vector<haylen::platform::HeadlessHost::AccessibilityTree>& trees = fixture.host().getAccessibilityTrees();
        if (trees.empty()) {
            throw std::runtime_error("The platform received no accessibility tree.");
        }

        return trees.back();
    }

    test::LuaFixture fixture;
};

TEST_F(AccessibilityTreeTest, DescribesEveryKindWithItsRoleLabelValueAndState) {
    // clang-format off
    fixture.runLua(R"(
        gui = ui.mount(ui.column{
            ui.pageHeader{title = 'Settings', caption = 'Audio'},
            ui.label{text = 'Choose how the game sounds.'},
            ui.button{id = 'play', text = 'Play'},
            ui.button{text = 'Locked', enabled = false},
            ui.toggle{text = 'Music', checked = true},
            ui.checkbox{text = 'Hints'},
            ui.slider{value = 0.25, decimals = 2, accessibilityLabel = 'Volume'},
            ui.textField{value = 'Ana', placeholder = 'Name'},
            ui.secretField{value = 'abc', placeholder = 'Password'},
            ui.radioGroup{items = {{id = 'easy', text = 'Easy'}, {id = 'hard', text = 'Hard'}}, selected = 'hard', accessibilityLabel = 'Difficulty'},
            ui.progress{value = 0.5, text = 'Download'},
            ui.image{image = 'missing.png'},
            ui.image{image = 'missing.png', accessibilityLabel = 'Map'},
            ui.richText{text = 'A [b]bold[/b] [lb]word[rb]'},
            ui.column{accessibilityHidden = true, ui.label{text = 'Hidden'}},
            ui.card{accessibilityLabel = 'Coins', accessibilityRole = 'text', accessibilityHint = 'Spent in the shop', ui.label{text = '12'}},
            ui.tree{items = {{id = 'maps', text = 'Maps', children = {{id = 'coast', text = 'Coast'}}}}, selected = 'coast', expanded = {'maps'}},
            ui.table{columns = {{text = 'Player'}, {text = 'Score'}}, rows = {{id = 'ana', cells = {'Ana', '120'}}}, selected = 'ana'},
        }, {placement = 'screen'})
    )");
    // clang-format on
    fixture.frames(3);

    const Element* header = find("Settings, Audio");
    ASSERT_NE(header, nullptr);
    EXPECT_EQ(header->role, Element::Role::Heading);
    EXPECT_EQ(get("Choose how the game sounds.").role, Element::Role::Text);
    const Element& play = get("Play");
    EXPECT_EQ(play.role, Element::Role::Button);
    EXPECT_EQ(play.actions, Element::Focus | Element::Activate);
    EXPECT_GT(play.bounds.width, 0.0F);
    EXPECT_EQ(get("Locked").states, Element::Disabled);
    EXPECT_EQ(get("Locked").actions, 0U);
    EXPECT_EQ(get("Music").role, Element::Role::Switch);
    EXPECT_TRUE(get("Music").has(Element::Checked));
    EXPECT_FALSE(get("Hints").has(Element::Checked));
    EXPECT_EQ(get("Volume").value, "0.25");
    EXPECT_TRUE(get("Volume").can(Element::Increment));
    EXPECT_EQ(get("Name").value, "Ana");
    EXPECT_EQ(get("Password").value, "•••");

    // The options of a radio group are elements inside the group.
    const Element& group = get("Difficulty");
    EXPECT_EQ(group.role, Element::Role::RadioGroup);
    EXPECT_EQ(get("Hard").parent, group.id);
    EXPECT_TRUE(get("Hard").has(Element::Checked));
    EXPECT_FALSE(get("Easy").has(Element::Checked));
    EXPECT_EQ(get("Download").value, "50%");

    // Images are decorative until they have a label, hidden nodes leave their children out and the app names roles and hints of its own.
    EXPECT_EQ(get("Map").role, Element::Role::Image);
    EXPECT_EQ(get("A bold [word]").role, Element::Role::Text);
    EXPECT_EQ(find("Hidden"), nullptr);
    const Element& coins = get("Coins");
    EXPECT_EQ(coins.role, Element::Role::Text);
    EXPECT_EQ(coins.hint, "Spent in the shop");
    EXPECT_EQ(get("12").parent, coins.id);
    std::size_t images = 0;
    for (const Element& element : getLastTree().elements) {
        images += element.role == Element::Role::Image ? 1U : 0U;
    }

    EXPECT_EQ(images, 1U);

    // The rows of trees and tables are elements of their own, with their text and their state.
    EXPECT_TRUE(get("Maps").has(Element::Expanded));
    EXPECT_TRUE(get("Coast").has(Element::Selected));
    EXPECT_TRUE(get("Ana, 120").has(Element::Selected));
}

TEST_F(AccessibilityTreeTest, SendsTheTreeOnlyWhenItChanges) {
    fixture.runLua("gui = ui.mount(ui.column{ui.label{id = 'score', text = 'Score 1'}}, {placement = 'screen'})");
    fixture.frames(3);
    const std::size_t sent = fixture.host().getAccessibilityTrees().size();
    EXPECT_GE(sent, 1U);
    fixture.frames(5);
    EXPECT_EQ(fixture.host().getAccessibilityTrees().size(), sent);

    fixture.runLua("gui:set('score', {text = 'Score 2'})");
    fixture.frames(2);
    EXPECT_EQ(fixture.host().getAccessibilityTrees().size(), sent + 1);
    EXPECT_NE(find("Score 2"), nullptr);

    // Nothing builds while neither the platform nor the app wants the tree.
    fixture.host().setAccessibilityTreeWanted(false);
    fixture.runLua("gui:set('score', {text = 'Score 3'})");
    fixture.frames(3);
    EXPECT_EQ(find("Score 3"), nullptr);
    fixture.runLua("accessibility.setTreeEnabled(true)");
    fixture.frames(3);
    EXPECT_NE(find("Score 3"), nullptr);
    EXPECT_EQ(fixture.lua("return tostring(accessibility.treeEnabled())"), "true");
}

TEST_F(AccessibilityTreeTest, PerformsTheActionsOfScreenReadersAndFollowsTheFocus) {
    // clang-format off
    fixture.runLua(R"(
        heard = {}
        gui = ui.mount(ui.column{
            ui.button{id = 'play', text = 'Play', onClick = function() heard[#heard + 1] = 'play' end},
            ui.slider{id = 'volume', value = 0.5, step = 0.1, accessibilityLabel = 'Volume', onChange = function(event) heard[#heard + 1] = string.format('%.1f', event.value) end},
            ui.radioGroup{id = 'mode', items = {{id = 'easy', text = 'Easy'}, {id = 'hard', text = 'Hard'}}, selected = 'easy', onChange = function(event) heard[#heard + 1] = event.value end},
        }, {placement = 'screen'})
    )");
    // clang-format on
    fixture.frames(3);

    // A platform thread asks through the relay, and the UI performs the action at its next frame.
    haylen::platform::AccessibilityRelay::perform({.element = get("Play").id, .action = Element::Activate});
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ' ')"), "play");
    EXPECT_EQ(fixture.lua("return ui.focused().id"), "play");
    EXPECT_TRUE(get("Play").has(Element::Focused));
    EXPECT_EQ(getLastTree().focused, get("Play").id);

    fixture.runLua("heard = {}");
    haylen::platform::AccessibilityRelay::perform({.element = get("Volume").id, .action = Element::Increment});
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ' ')"), "0.6");
    EXPECT_EQ(get("Volume").value, "0.60");
    EXPECT_EQ(fixture.lua("return ui.focused().id"), "volume");

    // Lua asks the way a screen reader does, and the focus of the UI moves the focus of the tree.
    fixture.runLua("heard = {} for _, element in ipairs(accessibility.elements()) do if element.label == 'Hard' then accessibility.perform(element.id, 'activate') end end");
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ' ')"), "hard");
    EXPECT_TRUE(get("Hard").has(Element::Checked));
    fixture.runLua("gui:command('play', 'focus')");
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("for _, element in ipairs(accessibility.elements()) do if element.focused then return element.label end end"), "Play");
    EXPECT_EQ(fixture.lua("return tostring(accessibility.focused() ~= nil)"), "true");

    // An action for an element that is gone does nothing.
    haylen::platform::AccessibilityRelay::perform({.element = 12345, .action = Element::Activate});
    fixture.frames(2);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_NE(fixture.lua("accessibility.perform(1, 'shake')").find("The accessibility action \"shake\" is unknown."), std::string::npos);
}

// Elements cover only what their scroll area shows, so a touch or a tap that looks for a row by its place never lands on the button above the area, while the rows out of sight stay in the tree and come into view when a screen reader moves to them.
TEST_F(AccessibilityTreeTest, CoversOnlyWhatTheScrollAreaShows) {
    // clang-format off
    fixture.runLua(R"(
        local rows = {}
        for index = 1, 10 do
            rows[index] = ui.button{id = 'row' .. index, text = 'Row ' .. index, height = 40}
        end
        gui = ui.mount(ui.column{gap = 0,
            ui.button{text = 'Search', height = 40},
            ui.scroll{id = 'list', height = 100, ui.column{gap = 0, children = rows}},
        }, {placement = 'screen'})
    )");
    // clang-format on
    fixture.frames(3);
    fixture.runLua("gui:command('list', 'scrollTo', {offset = 60, animated = false})");
    fixture.frames(3);

    // The area shows the bottom half of the second row, the third and fourth rows whole, and nothing of the first and the last rows, which lie on its edges.
    const haylen::math::Rect search = get("Search").bounds;
    const haylen::math::Rect first = get("Row 1").bounds;
    const haylen::math::Rect second = get("Row 2").bounds;
    const haylen::math::Rect third = get("Row 3").bounds;
    const haylen::math::Rect fourth = get("Row 4").bounds;
    const haylen::math::Rect last = get("Row 10").bounds;
    EXPECT_FLOAT_EQ(first.height, 0.0F);
    EXPECT_FLOAT_EQ(first.y, second.y);
    EXPECT_GE(first.y, search.getBottom());
    EXPECT_FLOAT_EQ(second.height * 2.0F, third.height);
    EXPECT_FLOAT_EQ(second.getBottom(), third.y);
    EXPECT_FLOAT_EQ(fourth.height, third.height);
    EXPECT_FLOAT_EQ(last.height, 0.0F);
    EXPECT_FLOAT_EQ(last.y, fourth.getBottom());
    EXPECT_TRUE(search.intersection(first).isEmpty());
    EXPECT_EQ(get("Row 1").actions, Element::Focus | Element::Activate);

    // A screen reader that moves to the first row focuses it, and the focus scrolls it into view.
    haylen::platform::AccessibilityRelay::perform({.element = get("Row 1").id, .action = Element::Focus});
    fixture.frames(30);
    EXPECT_EQ(fixture.lua("return ui.focused().id"), "row1");
    EXPECT_FLOAT_EQ(get("Row 1").bounds.height, third.height);
    EXPECT_FLOAT_EQ(get("Row 1").bounds.y, search.getBottom());
}

// A modal presentation hides the GUIs under it from screen readers, which reach its screen, the messages above it, the grabber of its sheet and the action of a message, though neither takes the focus.
TEST_F(AccessibilityTreeTest, ReachesThePresentationAndTheMessagesButNotWhatAModalCovers) {
    fixture.host().resize({480.0F, 1000.0F});
    // clang-format off
    fixture.runLua(R"(
        ui.setScaleMode('physical')
        undone = false
        gui = ui.mount(ui.column{ui.button{text = 'Under'}}, {placement = 'screen'})
        sheet = ui.present({build = function() return ui.column{ui.label{text = 'Queue'}} end}, {detents = {'medium', 'large'}})
        ui.showMessage{style = 'snackbar', text = 'The map was deleted.', action = 'Undo', duration = 0, onAction = function() undone = true end}
    )");
    // clang-format on
    fixture.frames(40);

    EXPECT_EQ(find("Under"), nullptr);
    EXPECT_NE(find("Queue"), nullptr);
    ASSERT_NE(find("The map was deleted. Undo"), nullptr);
    EXPECT_EQ(get("The map was deleted. Undo").role, Element::Role::Alert);
    ASSERT_NE(find("Sheet grabber"), nullptr);
    EXPECT_EQ(get("Sheet grabber").actions, Element::Activate);

    haylen::platform::AccessibilityRelay::perform({.element = get("Sheet grabber").id, .action = Element::Activate});
    fixture.frames(30);
    EXPECT_EQ(fixture.lua("return sheet.detent"), "2");

    haylen::platform::AccessibilityRelay::perform({.element = get("Undo").id, .action = Element::Activate});
    fixture.frames(30);
    EXPECT_EQ(fixture.lua("return tostring(undone)"), "true");

    fixture.runLua("sheet:dismiss()");
    fixture.frames(30);
    EXPECT_NE(find("Under"), nullptr);
}

// The answers of a dialog and of an alert reach screen readers and the focus in the order people read them, from the start of their row, while the main answer at the end starts with the focus.
TEST_F(AccessibilityTreeTest, ReadsTheAnswersOfADialogInTheOrderOfTheRow) {
    // clang-format off
    fixture.runLua(R"(
        gui = ui.mount(ui.dialog{title = 'Save the island?', buttons = {{id = 'cancel', text = 'Cancel'}, {id = 'later', text = 'Later'}, {id = 'save', text = 'Save', variant = 'primary'}}}, {placement = 'screen'})
    )");
    // clang-format on
    fixture.frames(3);

    const std::vector<haylen::platform::HeadlessHost::AccessibilityTree>& trees = fixture.host().getAccessibilityTrees();
    ASSERT_FALSE(trees.empty());
    std::vector<std::string> answers;
    for (const Element& element : trees.back().elements) {
        if (element.role == Element::Role::Button) {
            answers.push_back(element.label);
        }
    }

    EXPECT_EQ(answers, (std::vector<std::string>{"Cancel", "Later", "Save"}));
    EXPECT_LT(get("Cancel").bounds.x, get("Later").bounds.x);
    EXPECT_TRUE(get("Save").has(Element::Focused));
}

TEST_F(AccessibilityTreeTest, ReadsElementsAndAnnouncesFromLua) {
    fixture.runLua("gui = ui.mount(ui.column{ui.toggle{id = 'music', text = 'Music', checked = true}}, {placement = 'screen'})");
    fixture.frames(3);
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        for _, element in ipairs(accessibility.elements()) do
            if element.label == 'Music' then
                return table.concat({element.role, tostring(element.checked), tostring(element.disabled), table.concat(element.actions, ','), tostring(element.bounds.width > 0), tostring(element.parent)}, ' ')
            end
        end
    )"), "switch true false focus,activate true 0");
    // clang-format on

    fixture.runLua("accessibility.announce('Level 3 is ready.') accessibility.announce('Game over.', {interrupt = true})");
    const std::vector<haylen::platform::HeadlessHost::Announcement>& announcements = fixture.host().getAnnouncements();
    ASSERT_EQ(announcements.size(), 2U);
    EXPECT_EQ(announcements[0].text, "Level 3 is ready.");
    EXPECT_FALSE(announcements[0].interrupt);
    EXPECT_TRUE(announcements[1].interrupt);
    EXPECT_NE(fixture.lua("accessibility.announce('')").find("An announcement needs a text."), std::string::npos);
    EXPECT_NE(fixture.lua("accessibility.announce('Hi', {loud = true})").find("Unknown option \"loud\"."), std::string::npos);
    EXPECT_NE(fixture.lua("ui.mount(ui.label{text = 'x', accessibilityRole = 'robot'})").find("accessibilityRole"), std::string::npos);
}

} // namespace haylen::lua::ui
