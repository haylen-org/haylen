#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/input/Key.hpp"
#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/NavigationSplit.hpp"
#include "haylen/ui/NavigationStack.hpp"
#include "haylen/ui/NavigationTabs.hpp"
#include "haylen/ui/Presentation.hpp"
#include "support/AllocationTracker.hpp"
#include "support/RecordingScreen.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

// The frame of an app: a bar of 80 units on top with a button, and the middle under it, split into a list and a detail, where the screens lay out.
class NavigationTest : public ::testing::Test, public test::UiFixture {
  protected:
    explicit NavigationTest(std::map<std::string, std::string> files = {}) : UiFixture(std::move(files)) {
        frame = mount(R"({"kind": "column", "gap": 0, "children": [{"kind": "row", "id": "bar", "height": 80, "children": [{"kind": "button", "id": "home", "text": "Home"}]}, {"kind": "row", "id": "middle", "grow": 1, "gap": 0, "align": "stretch", "children": [{"kind": "column", "id": "list", "grow": 1, "align": "stretch"}, {"kind": "column", "id": "detail", "grow": 1, "align": "stretch"}]}]})");
    }

    std::shared_ptr<test::RecordingScreen> make(const std::string& name) {
        return std::make_shared<test::RecordingScreen>(getUi(), name, log);
    }

    [[nodiscard]] Navigation& getNavigation() {
        return getUi().getNavigation();
    }

    [[nodiscard]] Region getArea(const std::string& node) const {
        return {.gui = frame, .node = node};
    }

    [[nodiscard]] bool hasEntry(const std::string& entry) const {
        return std::ranges::find(log, entry) != log.end();
    }

    // Runs the frames of a transition of the default duration and one more.
    void settle() {
        frames(30);
    }

    std::shared_ptr<Gui> frame;
    std::vector<std::string> log;
};

TEST_F(NavigationTest, ShowsTheRootScreenInTheAreaOfItsStack) {
    const auto home = make("home");
    const std::shared_ptr<NavigationStack> stack = getNavigation().createStack(getArea("list"), home);
    frames(2);

    EXPECT_EQ(home->getState(), Screen::State::Shown);
    EXPECT_FLOAT_EQ(getBounds(*home->getGui(), "open").x, getBounds(*frame, "list").x);
    EXPECT_FLOAT_EQ(getBounds(*home->getGui(), "open").y, getBounds(*frame, "list").y);
    EXPECT_EQ(log, (std::vector<std::string>{"home:build", "home:appear", "home:appeared"}));

    clearEvents();
    click(*home->getGui(), "open");
    EXPECT_EQ(findLastEvent("click").id, "open");
}

// The frame never moves or rebuilds while the screens slide inside the area of the stack, which cuts them at its edges.
TEST_F(NavigationTest, SlidesOnlyTheScreensOfTheStackWhileTheFrameStays) {
    const auto home = make("home");
    const auto profile = make("profile");
    const std::shared_ptr<NavigationStack> stack = getNavigation().createStack(getArea("list"), home);
    frames(2);
    const math::Rect bar = getBounds(*frame, "home");
    const math::Rect list = getBounds(*frame, "list");

    stack->push(profile);
    frames(8);
    ASSERT_TRUE(stack->isTransitioning());
    EXPECT_EQ(getBounds(*frame, "home"), bar);
    EXPECT_GT(getBounds(*profile->getGui(), "open").x, list.x);
    EXPECT_LT(getBounds(*home->getGui(), "open").x, list.x);
    EXPECT_EQ(profile->getState(), Screen::State::Appearing);
    EXPECT_EQ(home->getState(), Screen::State::Disappearing);

    settle();
    EXPECT_FALSE(stack->isTransitioning());
    EXPECT_FLOAT_EQ(getBounds(*profile->getGui(), "open").x, list.x);
    EXPECT_EQ(home->getState(), Screen::State::Hidden);
    EXPECT_EQ(profile->getState(), Screen::State::Shown);
    EXPECT_TRUE(hasEntry("home:disappeared"));
    EXPECT_TRUE(hasEntry("profile:appeared"));
}

// The screens of a change take no input until it ends, while the frame keeps its input.
TEST_F(NavigationTest, TakesNoInputInTheScreensOfAChangeThatPlays) {
    const auto home = make("home");
    const std::shared_ptr<NavigationStack> stack = getNavigation().createStack(getArea("list"), home);
    frames(2);
    stack->push(make("profile"));
    frames(4);
    ASSERT_TRUE(stack->isTransitioning());

    clearEvents();
    click(getBounds(*home->getGui(), "open").getCenter());
    EXPECT_TRUE(getEventNames().empty());
    click(*frame, "home");
    EXPECT_EQ(findLastEvent("click").id, "home");
}

// A popped screen leaves for good, while the screen under it shows again with the GUI it built once.
TEST_F(NavigationTest, PopsTheTopScreenAndShowsTheScreenUnderItAgain) {
    const auto home = make("home");
    const auto profile = make("profile");
    const std::shared_ptr<NavigationStack> stack = getNavigation().createStack(getArea("list"), home);
    stack->push(profile);
    settle();
    const std::shared_ptr<Gui> profileGui = profile->getGui();

    stack->pop();
    settle();
    EXPECT_EQ(stack->size(), 1U);
    EXPECT_EQ(profile->getState(), Screen::State::Removed);
    EXPECT_FALSE(getUi().isMounted(*profileGui));
    EXPECT_TRUE(hasEntry("profile:remove"));
    EXPECT_EQ(home->getState(), Screen::State::Shown);
    EXPECT_EQ(home->builds, 1);
    EXPECT_EQ(profile->getStack(), nullptr);
}

// A deep link sets the whole stack at once, and the screens under the top build their GUIs only when they show.
TEST_F(NavigationTest, BuildsOnlyTheTopScreenOfADeepLink) {
    const auto home = make("home");
    const auto artist = make("artist");
    const auto album = make("album");
    const std::shared_ptr<NavigationStack> stack = getNavigation().createStack(getArea("list"));
    stack->setScreens({home, artist, album}, {.effect = NavigationTransition::Effect::None});
    frames(2);

    EXPECT_EQ(album->getState(), Screen::State::Shown);
    EXPECT_EQ(home->builds, 0);
    EXPECT_EQ(artist->builds, 0);

    stack->pop();
    settle();
    EXPECT_EQ(artist->builds, 1);
    EXPECT_EQ(artist->getState(), Screen::State::Shown);
}

// The same frame with a picture in the package that a screen shows, which arrives some frames after the screen first draws.
class NavigationPictureTest : public NavigationTest {
  protected:
    NavigationPictureTest() : NavigationTest({{"content/icons/square.svg", R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><rect width="100" height="100" fill="#3366FF"/></svg>)"}}) {}

    std::shared_ptr<test::RecordingScreen> makePicture(const std::string& name) {
        return std::make_shared<test::RecordingScreen>(getUi(), name, log, R"({"kind": "column", "children": [{"kind": "image", "id": "picture", "image": "icons/square.svg", "width": 64, "height": 64}]})");
    }
};

// A screen moves in only once its GUI laid out with every picture it shows, so no picture pops in while it slides.
TEST_F(NavigationPictureTest, HoldsATransitionUntilThePicturesOfTheScreenThatComesArrived) {
    const std::shared_ptr<NavigationStack> stack = getNavigation().createStack(getArea("list"), make("home"));
    frames(2);
    const math::Rect list = getBounds(*frame, "list");
    const auto gallery = makePicture("gallery");
    stack->push(gallery);
    frames(2);
    ASSERT_TRUE(stack->isTransitioning());
    EXPECT_FLOAT_EQ(getBounds(*gallery->getGui(), "picture").x, list.x + list.width);

    ASSERT_TRUE(getFixture().frameUntil([&] { return getBounds(*gallery->getGui(), "picture").x < list.x + list.width; }));
    EXPECT_TRUE(getUi().getContext().getImage("icons/square.svg", {64.0F, 64.0F}).isValid());
    settle();
    EXPECT_FALSE(stack->isTransitioning());
}

// A presentation rises only once its screen laid out with every picture it shows.
TEST_F(NavigationPictureTest, HoldsAPresentationUntilThePicturesOfItsScreenArrived) {
    frames(2);
    const auto sheet = makePicture("sheet");
    const std::shared_ptr<Presentation> shown = getNavigation().present(sheet, {.style = Presentation::Style::FullScreen});
    frames(2);
    const float bottom = getUi().getBackend().getLiftedDisplayRect().getBottom();
    EXPECT_GE(getBounds(*sheet->getGui(), "picture").y, bottom);

    ASSERT_TRUE(getFixture().frameUntil([&] { return getBounds(*sheet->getGui(), "picture").y < bottom; }));
    EXPECT_TRUE(getUi().getContext().getImage("icons/square.svg", {64.0F, 64.0F}).isValid());
    settle();
    EXPECT_FALSE(shown->isDismissed());
    EXPECT_EQ(sheet->getState(), Screen::State::Shown);
}

// A screen whose GUI never lays out, such as one whose area does not draw, still moves in once the change waited as long as it waits.
TEST_F(NavigationTest, StartsATransitionThatWaitedAsLongAsItWaits) {
    const std::shared_ptr<NavigationStack> stack = getNavigation().createStack(getArea("list"), make("home"));
    frames(2);
    frame->set("list", core::Json::parse(R"({"visible": false})"));
    frames(2);
    stack->push(make("profile"));
    frames(static_cast<int>(Navigation::kReadyWait * 60.0F) - 6);
    ASSERT_TRUE(stack->isTransitioning());
    EXPECT_FALSE(hasEntry("profile:appeared"));

    settle();
    settle();
    EXPECT_FALSE(stack->isTransitioning());
    EXPECT_TRUE(hasEntry("profile:appeared"));
}

TEST_F(NavigationTest, RefusesAScreenThatAnotherStackHolds) {
    const auto home = make("home");
    const std::shared_ptr<NavigationStack> first = getNavigation().createStack(getArea("list"), home);
    const std::shared_ptr<NavigationStack> second = getNavigation().createStack(getArea("detail"));
    EXPECT_THROW(second->push(home), std::invalid_argument);
    EXPECT_THROW(first->push(home), std::invalid_argument);
    EXPECT_THROW(getNavigation().createStack({.gui = frame, .node = "sidebar"}), std::invalid_argument);
}

// Each tab keeps its stack with the GUIs of its screens, and selecting the tab that shows goes back to its root.
TEST_F(NavigationTest, KeepsTheStackOfEveryTab) {
    const auto home = make("home");
    const auto search = make("search");
    const auto album = make("album");
    // clang-format off
    const std::shared_ptr<NavigationTabs> tabs = getNavigation().createTabs(getArea("list"), {
        {.id = "home", .root = [home] { return home; }},
        {.id = "search", .root = [search] { return search; }},
    }, "home");
    // clang-format on
    tabs->getCurrent()->push(album);
    settle();
    EXPECT_EQ(search->builds, 0);

    tabs->select("search");
    frames(2);
    EXPECT_EQ(album->getState(), Screen::State::Hidden);
    EXPECT_EQ(search->getState(), Screen::State::Shown);
    EXPECT_EQ(tabs->getStack("home")->size(), 2U);

    tabs->select("home");
    frames(2);
    EXPECT_EQ(album->getState(), Screen::State::Shown);
    EXPECT_EQ(album->builds, 1);
    EXPECT_EQ(search->getState(), Screen::State::Hidden);

    tabs->select("home");
    settle();
    EXPECT_EQ(tabs->getStack("home")->size(), 1U);
    EXPECT_THROW(tabs->select("library"), std::invalid_argument);
}

// Collapsed, the detail goes on top of the list as one stack, and it moves back to the detail area with its GUI once the split expands.
TEST_F(NavigationTest, MovesTheDetailOntoTheListWhenTheSplitCollapses) {
    const auto inbox = make("inbox");
    const auto chat = make("chat");
    const auto empty = make("empty");
    const std::shared_ptr<NavigationSplit> split = getNavigation().createSplit(getArea("list"), getArea("detail"), inbox, [empty] { return empty; }, false);
    frames(2);
    EXPECT_EQ(empty->getState(), Screen::State::Shown);

    split->showDetail(chat);
    frames(2);
    EXPECT_EQ(empty->getState(), Screen::State::Removed);
    EXPECT_FLOAT_EQ(getBounds(*chat->getGui(), "open").x, getBounds(*frame, "detail").x);
    EXPECT_EQ(inbox->getState(), Screen::State::Shown);

    split->setCollapsed(true);
    frames(2);
    EXPECT_EQ(split->getPrimary()->getTop(), chat);
    EXPECT_EQ(inbox->getState(), Screen::State::Hidden);
    EXPECT_FLOAT_EQ(getBounds(*chat->getGui(), "open").x, getBounds(*frame, "list").x);
    EXPECT_TRUE(split->hasDetail());

    split->setCollapsed(false);
    frames(2);
    EXPECT_EQ(split->getDetail()->getTop(), chat);
    EXPECT_EQ(chat->builds, 1);
    EXPECT_EQ(inbox->getState(), Screen::State::Shown);

    split->setCollapsed(true);
    split->clearDetail();
    settle();
    EXPECT_FALSE(split->hasDetail());
    EXPECT_EQ(chat->getState(), Screen::State::Removed);
    EXPECT_EQ(inbox->getState(), Screen::State::Shown);
}

// A presentation draws above everything, and the GUIs under it take no input until it leaves.
TEST_F(NavigationTest, PresentsAScreenThatTakesTheInputOfTheGuisUnderIt) {
    const auto compose = make("compose");
    const std::shared_ptr<Presentation> sheet = getNavigation().present(compose, {.style = Presentation::Style::FullScreen});
    settle();
    ASSERT_EQ(compose->getState(), Screen::State::Shown);

    clearEvents();
    click(getBounds(*frame, "home").getCenter());
    EXPECT_EQ(findLastEvent("click").id, "open");
    clearEvents();
    click(*compose->getGui(), "open");
    EXPECT_EQ(findLastEvent("click").id, "open");

    sheet->dismiss();
    settle();
    EXPECT_EQ(compose->getState(), Screen::State::Removed);
    EXPECT_TRUE(getNavigation().getPresentations().empty());
    clearEvents();
    click(*frame, "home");
    EXPECT_EQ(findLastEvent("click").id, "home");
}

// The system back pops the innermost stack, then goes to the first tab, then dismisses the presentation, and only then leaves the app.
TEST_F(NavigationTest, GoesBackThroughTheStacksTheTabsAndThePresentations) {
    const auto home = make("home");
    const auto search = make("search");
    // clang-format off
    const std::shared_ptr<NavigationTabs> tabs = getNavigation().createTabs(getArea("list"), {
        {.id = "home", .root = [home] { return home; }},
        {.id = "search", .root = [search] { return search; }},
    }, "search");
    // clang-format on
    tabs->getCurrent()->push(make("artist"));
    const std::shared_ptr<Presentation> sheet = getNavigation().present(make("compose"), {});
    settle();
    EXPECT_TRUE(getUi().isCapturingBack());

    key(input::Key::Escape);
    settle();
    EXPECT_TRUE(sheet->isDismissed());

    key(input::Key::Escape);
    settle();
    EXPECT_EQ(tabs->getCurrent()->size(), 1U);

    key(input::Key::Escape);
    settle();
    EXPECT_EQ(tabs->getSelected(), "home");
    EXPECT_FALSE(getUi().isCapturingBack());
}

TEST_F(NavigationTest, KeepsAScreenWhoseBackHookTakesTheBack) {
    const auto draft = make("draft");
    const std::shared_ptr<NavigationStack> stack = getNavigation().createStack(getArea("list"), make("home"));
    stack->push(draft);
    settle();
    draft->keeping = true;

    EXPECT_TRUE(getNavigation().goBack());
    settle();
    EXPECT_EQ(stack->getTop(), draft);
    EXPECT_TRUE(hasEntry("draft:back"));

    draft->keeping = false;
    EXPECT_TRUE(getNavigation().goBack());
    settle();
    EXPECT_EQ(stack->size(), 1U);
}

// A stack in the GUI of a screen shows and hides with that screen, and closes once that screen leaves.
TEST_F(NavigationTest, ShowsAStackInAScreenWithThatScreen) {
    const std::string hosting = R"({"kind": "column", "children": [{"kind": "column", "id": "inner", "grow": 1}]})";
    const auto messages = std::make_shared<test::RecordingScreen>(getUi(), "messages", log, hosting);
    const std::shared_ptr<NavigationStack> outer = getNavigation().createStack(getArea("list"), messages);
    frames(2);
    const auto chat = make("chat");
    const std::shared_ptr<NavigationStack> inner = getNavigation().createStack({.gui = messages->getGui(), .node = "inner"}, chat);
    frames(2);
    EXPECT_EQ(chat->getState(), Screen::State::Shown);

    outer->push(make("profile"));
    settle();
    EXPECT_EQ(chat->getState(), Screen::State::Hidden);

    outer->pop();
    settle();
    EXPECT_EQ(chat->getState(), Screen::State::Shown);

    outer->replace(make("settings"));
    settle();
    EXPECT_TRUE(inner->isClosed());
    EXPECT_EQ(chat->getState(), Screen::State::Removed);
}

TEST_F(NavigationTest, ClosesTheStacksOfAGuiThatUnmounts) {
    const auto home = make("home");
    const std::shared_ptr<NavigationStack> stack = getNavigation().createStack(getArea("list"), home);
    frames(2);
    getUi().unmount(*frame);
    EXPECT_TRUE(stack->isClosed());
    EXPECT_EQ(home->getState(), Screen::State::Removed);
}

// Screens that stay where they are cost the frames nothing more than the frame alone.
TEST_F(NavigationTest, ShowsStillScreensWithoutAllocating) {
    // clang-format off
    const auto measure = [this] {
        frames(5);
        const test::AllocationTracker tracker;
        frames(10);
        return tracker.getTotal();
    };
    // clang-format on
    const std::size_t frameAlone = measure();
    const std::shared_ptr<NavigationStack> stack = getNavigation().createStack(getArea("list"), make("home"));
    stack->push(make("profile"));
    const std::shared_ptr<Presentation> sheet = getNavigation().present(make("compose"), {});
    settle();
    EXPECT_EQ(measure(), frameAlone);
}

// Screens are tables of classes of `ui.Screen`, whose fields name their GUI and their stack, and the stack, the tabs and the presentation report their changes.
TEST_F(NavigationTest, NavigatesFromLua) {
    // clang-format off
    getFixture().runLua(R"(
        local haylen = require('haylen')
        ui = require('haylen.ui')
        log = {}
        Page = haylen.class('Page', ui.Screen)
        function Page:init(name) self.name = name end
        function Page:build() return ui.column{ui.button{id = 'open', text = self.name}} end
        function Page:appeared() log[#log + 1] = self.name .. ':appeared' end
        function Page:remove() log[#log + 1] = self.name .. ':remove' end
        frame = ui.mount(ui.column{gap = 0, ui.row{height = 80, ui.label{text = 'Bar'}}, ui.column{id = 'middle', grow = 1}, ui.column{id = 'side', grow = 1}}, {placement = 'screen'})
        changes = 0
        stack = ui.navigationStack{gui = frame, id = 'middle', root = Page('home'), onChange = function(event) changes = changes + 1 lastTop = event.top end}
        profile = Page('profile')
        stack:push(profile, {effect = 'fade', duration = 0.2})
    )");
    // clang-format on
    EXPECT_EQ(getFixture().lua("return tostring(profile.stack == stack) .. ui.screenState(profile) .. stack.size"), "trueappearing2");
    frames(20);
    EXPECT_EQ(getFixture().lua("return ui.screenState(profile) .. tostring(lastTop == profile) .. changes .. tostring(profile.gui.mounted)"), "showntrue2true");
    EXPECT_EQ(getFixture().lua("return tostring(stack.canGoBack) .. tostring(ui.canGoBack()) .. tostring(ui.goBack())"), "truetruetrue");
    frames(30);
    EXPECT_EQ(getFixture().lua("return ui.screenState(profile) .. tostring(profile.stack) .. table.concat(log, ',')"), "removednilhome:appeared,profile:appeared,home:appeared,profile:remove");

    // clang-format off
    getFixture().runLua(R"(
        tabs = ui.navigationTabs{gui = frame, id = 'side', tabs = {{id = 'a', root = function() return Page('a') end}, {id = 'b', root = function() return Page('b') end}}, onChange = function(event) picked = event.selected .. event.previous end}
        tabs:select('b')
        sheet = ui.present(Page('compose'), {style = 'dialog', onDismiss = function(event) closed = event.presentation == sheet end})
    )");
    // clang-format on
    frames(20);
    EXPECT_EQ(getFixture().lua("return picked .. tabs.current.top.name .. tostring(sheet.screen.presentation == sheet) .. sheet.style"), "babtruedialog");
    getFixture().runLua("sheet:dismiss()");
    frames(20);
    EXPECT_EQ(getFixture().lua("return tostring(closed) .. tostring(sheet.dismissed) .. ui.screenState(sheet.screen)"), "truetrueremoved");
}

TEST_F(NavigationTest, ReportsMistakesFromLua) {
    getFixture().runLua("ui = require('haylen.ui') frame = ui.mount(ui.column{ui.column{id = 'middle'}}, {placement = 'screen'}) stack = ui.navigationStack{gui = frame, id = 'middle'}");
    EXPECT_NE(getFixture().lua("ui.navigationStack{gui = frame, id = 'menu'}").find("no node with the id \"menu\""), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.navigationStack{gui = frame}").find("names the node of its area with \"id\""), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.navigationStack{gui = frame, id = 'middle', speed = 2}").find("Unknown option \"speed\""), std::string::npos);
    EXPECT_NE(getFixture().lua("stack:push(5)").find("A screen is a table with a \"build\" method, not number."), std::string::npos);
    EXPECT_NE(getFixture().lua("stack:push({build = function() end}, {speed = 1})").find("Unknown option \"speed\""), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.navigationTabs{gui = frame, id = 'middle', tabs = {}}").find("at least one tab"), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.present({build = function() end}, {style = 'popup'})").find("error: "), std::string::npos);
}

} // namespace haylen::ui
