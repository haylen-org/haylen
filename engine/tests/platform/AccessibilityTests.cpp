#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/platform/Accessibility.hpp"
#include "haylen/platform/AccessibilityElement.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

TEST(AccessibilityTest, PublishesEveryChangeOfTheSettingsOnce) {
    test::EngineFixture fixture;
    std::vector<std::string> log;
    fixture.engine().getEvents().on(haylen::core::LifecycleEvent::kAccessibilitySettingsChanged, [&log](haylen::core::EventBus::Event& event) { log.push_back(event.getData().dump()); });
    Accessibility& accessibility = fixture.engine().getAccessibility();
    EXPECT_EQ(accessibility.getSettings(), AccessibilitySettings{});

    // Platform services report from their own threads, and the engine hears the last report at its next frame.
    // clang-format off
    std::thread([&fixture] {
        fixture.host().setAccessibilitySettings({.textScale = 1.5F});
        fixture.host().setAccessibilitySettings({.textScale = 1.25F, .reducedMotion = true});
    }).join();
    // clang-format on
    EXPECT_EQ(accessibility.getSettings(), AccessibilitySettings{});
    fixture.frames(2);
    EXPECT_EQ(log, (std::vector<std::string>{R"({"highContrast":false,"reducedMotion":true,"reducedTransparency":false,"screenReader":false,"textScale":1.25})"}));
    EXPECT_EQ(accessibility.getSettings(), (AccessibilitySettings{.textScale = 1.25F, .reducedMotion = true}));

    // A new app starts with what the platform reported last, without an event.
    fixture.host().setAccessibilitySettings({.highContrast = true, .reducedTransparency = true, .screenReader = true});
    fixture.restart();
    std::vector<std::string> restarted;
    fixture.engine().getEvents().on(haylen::core::LifecycleEvent::kAccessibilitySettingsChanged, [&restarted](haylen::core::EventBus::Event& event) { restarted.push_back(event.getData().dump()); });
    fixture.frames(1);
    EXPECT_TRUE(restarted.empty());
    EXPECT_TRUE(fixture.engine().getAccessibility().getSettings().screenReader);
}

TEST(AccessibilityTest, WritesElementsAsTheJsonOfTheScreenReadersOfAndroidAndTheWeb) {
    const AccessibilityElement element{.id = 4294967295U, .parent = 7, .bounds = {10.0F, 20.5F, 30.0F, 40.0F}, .role = AccessibilityElement::Role::Switch, .label = "Music", .value = "", .hint = "Plays the songs", .states = AccessibilityElement::Checked, .actions = AccessibilityElement::Focus | AccessibilityElement::Activate};
    EXPECT_EQ(element.toJson().dump(), R"({"actions":3,"bounds":[10.0,20.5,30.0,40.0],"hint":"Plays the songs","id":4294967295,"label":"Music","parent":7,"role":"switch","states":2,"value":""})");
    EXPECT_EQ(AccessibilityElement::roleFromName("radioButton"), AccessibilityElement::Role::RadioButton);
    EXPECT_FALSE(AccessibilityElement::roleFromName("robot").has_value());
}

// The tree that Android and the web receive carries any text an app gives its elements, where bytes that are no UTF-8 become replacement characters instead of stopping the tree.
TEST(AccessibilityTest, WritesTheTreeOfElementsWhoseTextsAreNotUtf8) {
    const std::vector<AccessibilityElement> elements{{.id = 1, .role = AccessibilityElement::Role::Button, .label = "Caf\xE9"}, {.id = 2, .parent = 1, .label = "Play"}};
    const haylen::core::Json tree = haylen::core::Json::parse(AccessibilityElement::writeTree(elements, 2));
    EXPECT_EQ(tree.at("focused"), 2);
    EXPECT_EQ(tree.at("elements").at(0).at("label"), "Caf\xEF\xBF\xBD");
    EXPECT_EQ(tree.at("elements").at(1).at("parent"), 1);
}

} // namespace haylen::platform
