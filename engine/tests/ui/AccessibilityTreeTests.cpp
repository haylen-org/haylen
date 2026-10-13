#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/platform/Accessibility.hpp"
#include "platform/AccessibilityRelay.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::ui {

using Element = platform::AccessibilityElement;

class AccessibilityTreeTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.host().setAccessibilityTreeWanted(true);
        fixture.runLua("ui = require('haylen.ui') accessibility = require('haylen.accessibility')");
    }

    // Returns the element of the last tree the platform received whose label is the text.
    [[nodiscard]] const Element* find(std::string_view label) {
        const std::vector<platform::HeadlessHost::AccessibilityTree>& trees = fixture.host().getAccessibilityTrees();
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

    test::EngineFixture fixture;
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
        }, {placement = 'screen'})
    )");
    // clang-format on
    fixture.frames(3);

    const Element* header = find("Settings, Audio");
    ASSERT_NE(header, nullptr);
    EXPECT_EQ(header->role, Element::Role::Heading);
    EXPECT_EQ(find("Choose how the game sounds.")->role, Element::Role::Text);
    const Element* play = find("Play");
    EXPECT_EQ(play->role, Element::Role::Button);
    EXPECT_EQ(play->actions, Element::Focus | Element::Activate);
    EXPECT_GT(play->bounds.width, 0.0F);
    EXPECT_EQ(find("Locked")->states, Element::Disabled);
    EXPECT_EQ(find("Locked")->actions, 0U);
    EXPECT_EQ(find("Music")->role, Element::Role::Switch);
    EXPECT_TRUE(find("Music")->has(Element::Checked));
    EXPECT_FALSE(find("Hints")->has(Element::Checked));
    EXPECT_EQ(find("Volume")->value, "0.25");
    EXPECT_TRUE(find("Volume")->can(Element::Increment));
    EXPECT_EQ(find("Name")->value, "Ana");
    EXPECT_EQ(find("Password")->value, "•••");

    // The options of a radio group are elements inside the group.
    const Element* group = find("Difficulty");
    EXPECT_EQ(group->role, Element::Role::RadioGroup);
    EXPECT_EQ(find("Hard")->parent, group->id);
    EXPECT_TRUE(find("Hard")->has(Element::Checked));
    EXPECT_FALSE(find("Easy")->has(Element::Checked));
    EXPECT_EQ(find("Download")->value, "50%");

    // Images are decorative until they have a label, hidden nodes leave their children out and the app names roles and hints of its own.
    EXPECT_EQ(find("Map")->role, Element::Role::Image);
    EXPECT_EQ(find("A bold [word]")->role, Element::Role::Text);
    EXPECT_EQ(find("Hidden"), nullptr);
    const Element* coins = find("Coins");
    EXPECT_EQ(coins->role, Element::Role::Text);
    EXPECT_EQ(coins->hint, "Spent in the shop");
    EXPECT_EQ(find("12")->parent, coins->id);
    std::size_t images = 0;
    for (const Element& element : fixture.host().getAccessibilityTrees().back().elements) {
        images += element.role == Element::Role::Image ? 1U : 0U;
    }
    EXPECT_EQ(images, 1U);
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
    platform::AccessibilityRelay::perform({.element = find("Play")->id, .action = Element::Activate});
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ' ')"), "play");
    EXPECT_EQ(fixture.lua("return ui.focused().id"), "play");
    EXPECT_TRUE(find("Play")->has(Element::Focused));
    EXPECT_EQ(fixture.host().getAccessibilityTrees().back().focused, find("Play")->id);

    fixture.runLua("heard = {}");
    platform::AccessibilityRelay::perform({.element = find("Volume")->id, .action = Element::Increment});
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ' ')"), "0.6");
    EXPECT_EQ(find("Volume")->value, "0.60");
    EXPECT_EQ(fixture.lua("return ui.focused().id"), "volume");

    // Lua asks the way a screen reader does, and the focus of the UI moves the focus of the tree.
    fixture.runLua("heard = {} for _, element in ipairs(accessibility.elements()) do if element.label == 'Hard' then accessibility.perform(element.id, 'activate') end end");
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ' ')"), "hard");
    EXPECT_TRUE(find("Hard")->has(Element::Checked));
    fixture.runLua("gui:command('play', 'focus')");
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("for _, element in ipairs(accessibility.elements()) do if element.focused then return element.label end end"), "Play");
    EXPECT_EQ(fixture.lua("return tostring(accessibility.focused() ~= nil)"), "true");

    // An action for an element that is gone does nothing.
    platform::AccessibilityRelay::perform({.element = 12345, .action = Element::Activate});
    fixture.frames(2);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_NE(fixture.lua("accessibility.perform(1, 'shake')").find("The accessibility action \"shake\" is unknown."), std::string::npos);
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
    const std::vector<platform::HeadlessHost::Announcement>& announcements = fixture.host().getAnnouncements();
    ASSERT_EQ(announcements.size(), 2U);
    EXPECT_EQ(announcements[0].text, "Level 3 is ready.");
    EXPECT_FALSE(announcements[0].interrupt);
    EXPECT_TRUE(announcements[1].interrupt);
    EXPECT_NE(fixture.lua("accessibility.announce('')").find("An announcement needs a text."), std::string::npos);
    EXPECT_NE(fixture.lua("accessibility.announce('Hi', {loud = true})").find("Unknown option \"loud\"."), std::string::npos);
    EXPECT_NE(fixture.lua("ui.mount(ui.label{text = 'x', accessibilityRole = 'robot'})").find("accessibilityRole"), std::string::npos);
}

} // namespace haylen::ui
