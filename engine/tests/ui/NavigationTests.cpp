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
#include "haylen/text/Direction.hpp"
#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/NavigationSplit.hpp"
#include "haylen/ui/NavigationStack.hpp"
#include "haylen/ui/NavigationTabs.hpp"
#include "haylen/ui/Presentation.hpp"
#include "haylen/ui/Scaling.hpp"
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

    // Makes the window as narrow as a phone in portrait, with one UI unit for each point of the screen.
    void usePhone() {
        getFixture().host().resize({480.0F, 1000.0F});
        getUi().setScaling({.mode = Scaling::Mode::Physical, .factor = 1.0F});
        frames(3);
    }

    // Press and drag the pointer at points in UI units.
    void clickUi(math::Vec2 point) {
        click(point * getUi().getBackend().getScale());
    }
    void dragUi(math::Vec2 from, math::Vec2 to) {
        drag(from * getUi().getBackend().getScale(), to * getUi().getBackend().getScale(), 8);
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
    EXPECT_THROW(static_cast<void>(getNavigation().createStack({.gui = frame, .node = "sidebar"})), std::invalid_argument);
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

// A sheet on a compact screen rests at its detent, moves to another one when the person drags its grabber, and leaves when dragged below its lowest one, once its screen agreed.
TEST_F(NavigationTest, DragsASheetBetweenItsDetentsAndAwayBelowTheLowest) {
    usePhone();
    const auto queue = make("queue");
    const std::shared_ptr<Presentation> sheet = getNavigation().present(queue, {.detents = {{.kind = Presentation::Detent::Kind::Medium}, {.kind = Presentation::Detent::Kind::Large}}});
    settle();
    const math::Rect display = getUi().getBackend().getLiftedDisplayRect();
    const math::Rect safe = getUi().getBackend().getLiftedSafeRect();
    const float medium = display.getBottom() - display.height * 0.5F;
    const float large = safe.y + Presentation::kSheetTop;
    EXPECT_NEAR(getBounds(*queue->getGui(), "open").y, medium + Presentation::kGrabberBand, 1.0F);

    const math::Vec2 grabber{display.getCenter().x, medium + Presentation::kGrabberBand * 0.5F};
    dragUi(grabber, {grabber.x, safe.y + 40.0F});
    settle();
    EXPECT_EQ(sheet->getDetent(), 1U);
    EXPECT_NEAR(getBounds(*queue->getGui(), "open").y, large + Presentation::kGrabberBand, 1.0F);

    // A tap on the grabber goes to the next detent, and from the highest one back to the lowest.
    clickUi({grabber.x, large + Presentation::kGrabberBand * 0.5F});
    settle();
    EXPECT_EQ(sheet->getDetent(), 0U);

    dragUi(grabber, {grabber.x, display.getBottom() - 4.0F});
    settle();
    EXPECT_TRUE(hasEntry("queue:back"));
    EXPECT_TRUE(sheet->isDismissed());
    EXPECT_EQ(queue->getState(), Screen::State::Removed);
}

// A screen that keeps its place, such as one with unsaved work, refuses every dismissal by the person, and its sheet glides back to its detent.
TEST_F(NavigationTest, SpringsBackASheetWhoseScreenKeepsItsPlace) {
    usePhone();
    const auto draft = make("draft");
    draft->keeping = true;
    const std::shared_ptr<Presentation> sheet = getNavigation().present(draft, {.grabber = true});
    settle();
    const math::Rect display = getUi().getBackend().getLiftedDisplayRect();
    const float rest = getBounds(*draft->getGui(), "open").y;
    const math::Vec2 grabber{display.getCenter().x, rest - Presentation::kGrabberBand * 0.5F};

    dragUi(grabber, {grabber.x, display.getBottom() - 4.0F});
    settle();
    EXPECT_FALSE(sheet->isDismissed());
    EXPECT_NEAR(getBounds(*draft->getGui(), "open").y, rest, 1.0F);

    clickUi({display.getCenter().x, display.y + 4.0F});
    key(input::Key::Escape);
    settle();
    EXPECT_FALSE(sheet->isDismissed());
    EXPECT_EQ(std::ranges::count(log, std::string("draft:back")), 3);
}

// A presented flow hands its result to whoever presented it, and a dismissal by the person leaves no result.
TEST_F(NavigationTest, ReturnsTheResultOfAPresentedFlow) {
    const std::shared_ptr<Presentation> picker = getNavigation().present(make("picker"), {});
    core::Json heard;
    static_cast<void>(picker->dismissed.connect([&heard](Presentation& left) { heard = left.getResult(); }));
    settle();
    picker->dismiss(core::Json{{"color", "teal"}});
    settle();
    EXPECT_EQ(heard, (core::Json{{"color", "teal"}}));

    const std::shared_ptr<Presentation> other = getNavigation().present(make("other"), {});
    settle();
    key(input::Key::Escape);
    settle();
    EXPECT_TRUE(other->isDismissed());
    EXPECT_TRUE(other->getResult().is_null());
}

// An alert leaves with the action the person chose, the back chooses its cancel action, and an alert without one stays until one of its actions closes it.
TEST_F(NavigationTest, PresentsAlertsWhoseChosenActionIsTheResult) {
    const Navigation::Alert discard{.title = "Discard the draft?", .actions = {{.id = "keep", .text = "Keep", .role = Navigation::Alert::Role::Cancel}, {.id = "discard", .text = "Discard", .role = Navigation::Alert::Role::Destructive}}};
    const std::shared_ptr<Presentation> chosen = getNavigation().presentAlert(discard);
    settle();
    key(input::Key::Enter);
    settle();
    EXPECT_EQ(chosen->getResult(), "discard");

    const std::shared_ptr<Presentation> cancelled = getNavigation().presentAlert(discard);
    settle();
    key(input::Key::Escape);
    settle();
    EXPECT_EQ(cancelled->getResult(), "keep");

    const std::shared_ptr<Presentation> required = getNavigation().presentAlert({.title = "Update required", .actions = {{.id = "update", .text = "Update"}}});
    settle();
    key(input::Key::Escape);
    settle();
    EXPECT_FALSE(required->isDismissed());
    key(input::Key::Enter);
    settle();
    EXPECT_EQ(required->getResult(), "update");

    EXPECT_THROW(static_cast<void>(getNavigation().presentAlert({.title = "Empty"})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(getNavigation().presentAlert({.actions = {{.id = "a"}, {.id = "a"}}})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(getNavigation().presentAlert({.actions = {{.id = "a", .role = Navigation::Alert::Role::Cancel}, {.id = "b", .role = Navigation::Alert::Role::Cancel}}})), std::invalid_argument);
}

// An action sheet points at its control from under it on a wide screen, rises from the bottom of a compact one, and a tap outside it chooses its cancel action.
TEST_F(NavigationTest, ShowsAnActionSheetNextToItsAnchorOrFromTheBottom) {
    const Navigation::Alert sharing{.style = Navigation::Alert::Style::ActionSheet, .title = "Share", .actions = {{.id = "copy", .text = "Copy link"}, {.id = "cancel", .text = "Cancel", .role = Navigation::Alert::Role::Cancel}}, .anchor = getArea("home")};
    const std::shared_ptr<Presentation> wide = getNavigation().presentAlert(sharing);
    settle();
    const math::Rect anchor = getBounds(*frame, "home");
    const math::Rect copy = getBounds(*wide->getScreen()->getGui(), "copy");
    EXPECT_EQ(wide->getStyle(), Presentation::Style::Popover);
    EXPECT_GT(copy.y, anchor.getBottom() + Presentation::kArrowSize);
    EXPECT_LT(copy.y, anchor.getBottom() + 300.0F);
    clickUi(copy.getCenter());
    settle();
    EXPECT_EQ(wide->getResult(), "copy");

    usePhone();
    const std::shared_ptr<Presentation> compact = getNavigation().presentAlert(sharing);
    settle();
    const math::Rect display = getUi().getBackend().getLiftedDisplayRect();
    EXPECT_GT(getBounds(*compact->getScreen()->getGui(), "copy").y, display.getCenter().y);
    clickUi({display.getCenter().x, display.y + 8.0F});
    settle();
    EXPECT_EQ(compact->getResult(), "cancel");
}

// A popover floats under the control it points at with its arrow at the middle of the control, and becomes a sheet across the width of a compact screen.
TEST_F(NavigationTest, PointsAPopoverAtItsAnchorAndMakesItASheetOnACompactScreen) {
    const auto info = make("info");
    const std::shared_ptr<Presentation> popover = getNavigation().present(info, {.style = Presentation::Style::Popover, .width = 400.0F, .anchor = getArea("home")});
    settle();
    const math::Rect anchor = getBounds(*frame, "home");
    const math::Rect content = getBounds(*info->getGui(), "open");
    EXPECT_FLOAT_EQ(content.y, anchor.getBottom() + Presentation::kArrowSize);
    EXPECT_FLOAT_EQ(content.width, 400.0F);
    EXPECT_LE(content.x, anchor.getCenter().x);
    popover->dismiss();
    settle();

    usePhone();
    const auto adapted = make("adapted");
    static_cast<void>(getNavigation().present(adapted, {.style = Presentation::Style::Popover, .anchor = getArea("home")}));
    settle();
    const math::Rect safe = getUi().getBackend().getLiftedSafeRect();
    EXPECT_FLOAT_EQ(getBounds(*adapted->getGui(), "open").x, safe.x);
    EXPECT_FLOAT_EQ(getBounds(*adapted->getGui(), "open").width, safe.width);
    EXPECT_THROW(static_cast<void>(getNavigation().present(make("lost"), {.style = Presentation::Style::Popover})), std::invalid_argument);
}

// A drawer slides in from the start edge, the right one in a right to left interface, and a drag toward that edge takes it away.
TEST_F(NavigationTest, SlidesADrawerFromTheStartEdgeAndOutByADrag) {
    const std::string tree = R"({"kind": "column", "children": [{"kind": "button", "id": "open", "text": "Inbox"}]})";
    const auto menu = std::make_shared<test::RecordingScreen>(getUi(), "menu", log, tree);
    const std::shared_ptr<Presentation> drawer = getNavigation().present(menu, {.style = Presentation::Style::Drawer, .width = 500.0F});
    frames(8);
    const math::Rect safe = getUi().getBackend().getLiftedSafeRect();
    EXPECT_LT(getBounds(*menu->getGui(), "open").x, safe.x);
    settle();
    EXPECT_FLOAT_EQ(getBounds(*menu->getGui(), "open").x, safe.x);

    const math::Vec2 inside{safe.x + 250.0F, safe.getCenter().y};
    dragUi(inside, {inside.x - 300.0F, inside.y});
    settle();
    EXPECT_TRUE(drawer->isDismissed());

    getUi().setDirection(text::Direction::RightToLeft);
    const auto mirrored = std::make_shared<test::RecordingScreen>(getUi(), "mirrored", log, tree);
    const std::shared_ptr<Presentation> start = getNavigation().present(mirrored, {.style = Presentation::Style::Drawer, .width = 500.0F});
    settle();
    EXPECT_FLOAT_EQ(getBounds(*mirrored->getGui(), "open").getRight(), safe.getRight());
    clickUi({safe.x + 8.0F, safe.getCenter().y});
    settle();
    EXPECT_TRUE(start->isDismissed());
}

// While the system asks for reduced motion, a drawer fades in where it rests instead of sliding.
TEST_F(NavigationTest, FadesADrawerInPlaceWhileMotionIsReduced) {
    getFixture().host().setAccessibilitySettings({.reducedMotion = true});
    frames(2);
    const auto menu = make("menu");
    static_cast<void>(getNavigation().present(menu, {.style = Presentation::Style::Drawer}));
    frames(4);
    EXPECT_FLOAT_EQ(getBounds(*menu->getGui(), "open").x, getUi().getBackend().getLiftedSafeRect().x);
}

// A sheet that is not modal leaves the frame its input, and the back moves it down to its lowest detent rather than dismissing it.
TEST_F(NavigationTest, LeavesTheAppItsInputUnderASheetThatIsNotModal) {
    usePhone();
    const auto queue = make("queue");
    const std::shared_ptr<Presentation> sheet = getNavigation().present(queue, {.modal = false, .detents = {{.kind = Presentation::Detent::Kind::Height, .height = 200.0F}, {.kind = Presentation::Detent::Kind::Large}}});
    settle();
    clearEvents();
    clickUi(getBounds(*frame, "home").getCenter());
    EXPECT_EQ(findLastEvent("click").id, "home");
    clearEvents();
    clickUi(getBounds(*queue->getGui(), "open").getCenter());
    EXPECT_EQ(findLastEvent("click").id, "open");

    sheet->setDetent(1);
    settle();
    EXPECT_TRUE(getNavigation().canGoBack());
    EXPECT_TRUE(getNavigation().goBack());
    settle();
    EXPECT_EQ(sheet->getDetent(), 0U);
    EXPECT_FALSE(getNavigation().canGoBack());
    EXPECT_FALSE(getNavigation().goBack());
    EXPECT_FALSE(sheet->isDismissed());
    EXPECT_THROW(static_cast<void>(getNavigation().present(make("card"), {.style = Presentation::Style::Dialog, .modal = false})), std::invalid_argument);
}

// The GUIs behind a modal sheet shrink as it nears the top of a compact screen, and stay whole behind a sheet on a wide screen or one that is not modal.
TEST_F(NavigationTest, ShrinksTheGuisBehindASheetNearTheTopOfACompactScreen) {
    const Presentation::Frame phone{.display = {0.0F, 0.0F, 800.0F, 1600.0F}, .safe = {0.0F, 60.0F, 800.0F, 1500.0F}, .radius = 16.0F};
    const Presentation::Frame tablet{.display = {0.0F, 0.0F, 2000.0F, 1500.0F}, .safe = {0.0F, 40.0F, 2000.0F, 1440.0F}, .radius = 16.0F};
    const std::vector<Presentation::Detent> detents{{.kind = Presentation::Detent::Kind::Medium}, {.kind = Presentation::Detent::Kind::Large}};
    const std::shared_ptr<Presentation> half = getNavigation().present(make("half"), {.detents = detents});
    const std::shared_ptr<Presentation> full = getNavigation().present(make("full"), {.detents = detents, .detent = 1});
    const std::shared_ptr<Presentation> beside = getNavigation().present(make("beside"), {.modal = false, .detents = detents, .detent = 1});
    EXPECT_FLOAT_EQ(half->getLayout(phone).behindScale, 1.0F);
    EXPECT_FLOAT_EQ(full->getLayout(phone).behindScale, Presentation::kBehindScale);
    EXPECT_FLOAT_EQ(full->getLayout(tablet).behindScale, 1.0F);
    EXPECT_FLOAT_EQ(beside->getLayout(phone).behindScale, 1.0F);
    EXPECT_FLOAT_EQ(full->getLayout(phone).surface.y, phone.safe.y + Presentation::kSheetTop);
    EXPECT_FLOAT_EQ(full->getLayout(tablet).surface.width, Presentation::kSheetWidth);
}

// A screen narrower than a form sheet in points is compact whatever the scale of its UI, such as a phone that lays out a wide design, and the sheets and popovers fill its width.
TEST_F(NavigationTest, MakesAScreenCompactByItsWidthInPoints) {
    const Presentation::Frame phone{.display = {0.0F, 0.0F, 1920.0F, 4000.0F}, .safe = {0.0F, 0.0F, 1920.0F, 4000.0F}, .pointsPerUnit = 0.2F, .radius = 16.0F};
    const Presentation::Frame tablet{.display = {0.0F, 0.0F, 1920.0F, 1300.0F}, .safe = {0.0F, 0.0F, 1920.0F, 1300.0F}, .pointsPerUnit = 0.6F, .radius = 16.0F};
    const std::shared_ptr<Presentation> sheet = getNavigation().present(make("sheet"), {});
    const std::shared_ptr<Presentation> popover = getNavigation().present(make("popover"), {.style = Presentation::Style::Popover, .anchor = getArea("home")});
    EXPECT_TRUE(sheet->getLayout(phone).compact);
    EXPECT_FLOAT_EQ(sheet->getLayout(phone).surface.width, phone.display.width);
    EXPECT_FALSE(popover->getLayout(phone).arrow.has_value());
    EXPECT_FALSE(sheet->getLayout(tablet).compact);
    EXPECT_FLOAT_EQ(sheet->getLayout(tablet).surface.width, Presentation::kSheetWidth);
}

// A form sheet on a wide screen rests at no detent, so it shows no grabber and a new detent leaves it where it is, and it is as tall as its screen when every detent fits its screen.
TEST_F(NavigationTest, FloatsACardWithoutDetentsThatFitsItsScreen) {
    const Presentation::Frame tablet{.display = {0.0F, 0.0F, 2000.0F, 1500.0F}, .safe = {0.0F, 40.0F, 2000.0F, 1440.0F}, .radius = 16.0F, .contentHeight = 300.0F};
    const std::vector<Presentation::Detent> detents{{.kind = Presentation::Detent::Kind::Medium}, {.kind = Presentation::Detent::Kind::Large}};
    const auto queue = make("queue");
    const std::shared_ptr<Presentation> card = getNavigation().present(queue, {.detents = detents});
    const std::shared_ptr<Presentation> picker = getNavigation().present(make("picker"), {.detents = {{.kind = Presentation::Detent::Kind::Fit}}});
    EXPECT_TRUE(card->getLayout(tablet).grabber.isEmpty());
    EXPECT_FLOAT_EQ(card->getLayout(tablet).area.y, card->getLayout(tablet).surface.y);
    EXPECT_FLOAT_EQ(card->getLayout(tablet).surface.height, Presentation::kSheetHeight);
    EXPECT_FLOAT_EQ(picker->getLayout(tablet).surface.height, 300.0F);

    picker->dismiss();
    settle();
    const float rest = getBounds(*queue->getGui(), "open").y;
    card->setDetent(1);
    frames(2);
    EXPECT_FLOAT_EQ(getBounds(*queue->getGui(), "open").y, rest);
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
    const std::shared_ptr<Presentation> queue = getNavigation().present(make("queue"), {.modal = false, .detents = {{.kind = Presentation::Detent::Kind::Medium}, {.kind = Presentation::Detent::Kind::Large}}});
    const std::shared_ptr<Presentation> sheet = getNavigation().present(make("compose"), {.detents = {{.kind = Presentation::Detent::Kind::Fit}, {.kind = Presentation::Detent::Kind::Large}}, .detent = 1});
    const std::shared_ptr<Presentation> popover = getNavigation().present(make("info"), {.style = Presentation::Style::Popover, .anchor = getArea("home")});
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

// A sheet reports its detents and changes them from Lua, a presented flow hands its result to the promise of its presentation, and an alert resolves with the action the person chose.
TEST_F(NavigationTest, PresentsSheetsAlertsAndTheirResultsFromLua) {
    // clang-format off
    getFixture().runLua(R"(
        local async = require('async')
        ui = require('haylen.ui')
        Page = {build = function() return ui.column{ui.button{id = 'open', text = 'Open'}} end}
        frame = ui.mount(ui.column{ui.button{id = 'share', text = 'Share'}}, {placement = 'screen'})
        sheet = ui.present(Page, {detents = {'medium', 'large', 320}, detent = 2, onDetentChange = function(event) moved = event.detent end, onDismiss = function(event) left = event.result.color end})
        async.spawn(function() picked = sheet.result:await().color end)
        alert = ui.presentAlert{title = 'Delete the song?', actions = {{id = 'keep', text = 'Keep', role = 'cancel'}, {id = 'delete', text = 'Delete', role = 'destructive'}}}
        async.spawn(function() answer = alert.result:await() end)
    )");
    // clang-format on
    frames(30);
    EXPECT_EQ(getFixture().lua("return sheet.detent .. ' ' .. tostring(sheet.modal) .. ' ' .. sheet.style"), "2 true sheet");
    getFixture().runLua("sheet.detent = 1");
    EXPECT_EQ(getFixture().lua("return tostring(moved)"), "1");

    key(input::Key::Escape);
    frames(30);
    EXPECT_EQ(getFixture().lua("return tostring(answer)"), "keep");
    getFixture().runLua("sheet:dismiss({result = {color = 'teal'}, transition = {effect = 'fade', duration = 0.1}})");
    frames(30);
    EXPECT_EQ(getFixture().lua("return picked .. ' ' .. left .. ' ' .. tostring(sheet.dismissed)"), "teal teal true");

    // clang-format off
    getFixture().runLua(R"(
        menu = ui.presentAlert{style = 'actionSheet', anchor = {gui = frame, id = 'share'}, actions = {{id = 'copy', text = 'Copy link'}}}
        side = ui.present(Page, {style = 'drawer', width = 500})
    )");
    // clang-format on
    frames(30);
    EXPECT_EQ(getFixture().lua("return menu.style .. ' ' .. side.style .. ' ' .. tostring(menu.dismissible)"), "popover drawer true");
    EXPECT_NE(getFixture().lua("ui.present(Page, {detents = {'half'}})").find("A detent is \"medium\", \"large\", \"fit\" or a height in UI units, not half."), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.present(Page, {detents = {'large'}, detent = 3})").find("from 1 to 1"), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.present({build = Page.build}, {style = 'popover'})").find("A popover needs the node it points at as its anchor."), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.presentAlert{title = 'Hi'}").find("An alert lists its actions in \"actions\""), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.presentAlert{actions = {{id = 'ok', role = 'maybe'}}}").find("maybe"), std::string::npos);
    EXPECT_NE(getFixture().lua("side:dismiss({result = function() end})").find("cannot be converted to JSON"), std::string::npos);
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
