#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/ScopedConnection.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/NavigationSplit.hpp"
#include "haylen/ui/NavigationStack.hpp"
#include "haylen/ui/NavigationTabs.hpp"
#include "haylen/ui/Router.hpp"
#include "haylen/ui/Screen.hpp"
#include "support/RecordingScreen.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

// A frame with a middle where the tabs of the routes show, and screens that remember the parameters they were made from.
class RouterTest : public ::testing::Test, public test::UiFixture {
  protected:
    // A screen that creates a split of its list and detail the first time it shows, as the screen of a tab of messages does.
    class SplitScreen final : public Screen {
      public:
        SplitScreen(plugins::UiPlugin& owner, std::shared_ptr<Screen> list) : ui(owner), root(std::move(list)) {}

        [[nodiscard]] std::shared_ptr<Gui> build(core::Engine&, Placement placement) override {
            return ui.createGui(core::Json::parse(R"({"kind": "row", "children": [{"kind": "column", "id": "list", "grow": 1}, {"kind": "column", "id": "detail", "grow": 1}]})"), placement);
        }

        void appear(core::Engine&) override {
            if (!split) {
                split = ui.getNavigation().createSplit({.gui = getGui(), .node = "list"}, {.gui = getGui(), .node = "detail"}, root, {}, false);
            }
        }

        std::shared_ptr<NavigationSplit> split;

      private:
        plugins::UiPlugin& ui;
        std::shared_ptr<Screen> root;
    };

    RouterTest() {
        frame = mount(R"({"kind": "column", "children": [{"kind": "column", "id": "middle", "grow": 1, "align": "stretch"}]})");
    }

    // A screen whose name holds the parameters it was made from, and whose GUI holds the areas of a split.
    std::shared_ptr<test::RecordingScreen> make(const std::string& name) {
        return std::make_shared<test::RecordingScreen>(getUi(), name, log, R"({"kind": "row", "children": [{"kind": "column", "id": "list", "grow": 1}, {"kind": "column", "id": "detail", "grow": 1}]})");
    }

    Router::Factory named(const std::string& prefix, const char* key = nullptr) {
        // clang-format off
        return [this, prefix, key](const core::Json& parameters) {
            const std::string name = key == nullptr ? prefix : prefix + ":" + parameters.at(key).dump();
            return make(name);
        };
        // clang-format on
    }

    [[nodiscard]] Navigation& getNavigation() {
        return getUi().getNavigation();
    }

    // Tabs for the home and the search, whose roots are screens of their names.
    std::shared_ptr<NavigationTabs> createTabs() {
        // clang-format off
        return getNavigation().createTabs({.gui = frame, .node = "middle"}, {
            {.id = "home", .root = [this] { return make("home"); }},
            {.id = "search", .root = [this] { return make("search"); }},
        }, "home");
        // clang-format on
    }

    std::vector<Router::Route> createRoutes() {
        return {
            {.name = "home", .path = "home", .tab = "home"},
            {.name = "artist", .path = "artist/{artist}", .tab = "search", .screen = named("artist", "artist")},
            {.name = "album", .path = "artist/{artist}/album/{id:integer}", .query = {{"track", Router::Type::Integer}}, .parent = "artist", .screen = named("album", "id")},
            {.name = "player", .path = "player", .presentation = Presentation::Options{.style = Presentation::Style::FullScreen}, .screen = named("player")},
        };
    }

    // The names of the screens of a stack, from the root to the top.
    [[nodiscard]] static std::string describe(const NavigationStack& stack) {
        std::string names;
        for (const std::shared_ptr<Screen>& screen : stack.getScreens()) {
            names += (names.empty() ? "" : " ") + std::static_pointer_cast<test::RecordingScreen>(screen)->getName();
        }

        return names;
    }

    std::shared_ptr<Gui> frame;
    std::vector<std::string> log;
};

// A path matches with its typed parameters, a link of a scheme of the app reads its host as the first segment, a web link of a host of the app reads its path, and a segment of another type matches no route.
TEST_F(RouterTest, MatchesLinksWithTheirTypedParameters) {
    const std::shared_ptr<Router> router = getNavigation().createRouter({.routes = createRoutes(), .tabs = createTabs(), .schemes = {"music"}, .hosts = {"music.example.com"}});
    const std::optional<Router::Match> album = router->match("music://artist/sea%20wolves/album/7?track=3&utm=mail#top");
    ASSERT_TRUE(album.has_value());
    EXPECT_EQ(album->route->name, "album");
    EXPECT_EQ(album->parameters, (core::Json{{"artist", "sea wolves"}, {"id", 7}, {"track", 3}}));

    const std::optional<Router::Match> artist = router->match("https://music.example.com/artist/42");
    ASSERT_TRUE(artist.has_value());
    EXPECT_EQ(artist->parameters, (core::Json{{"artist", "42"}}));
    const std::optional<Router::Match> home = router->match("/home");
    ASSERT_TRUE(home.has_value());
    EXPECT_EQ(home->route->name, "home");
    EXPECT_FALSE(router->match("music://artist/42/album/seven").has_value());
    EXPECT_FALSE(router->match("photos://artist/42").has_value());
    EXPECT_FALSE(router->match("https://other.example.com/artist/42").has_value());
    EXPECT_THROW(static_cast<void>(router->match("artist/42/album/7?track=first")), std::invalid_argument);
}

// A link selects the tab of its route and builds its stack from the root of the tab, the screens of its parents and its own screen, and a link of a presentation presents its screen over them.
TEST_F(RouterTest, OpensALinkWithTheScreensOfItsParentsInItsTab) {
    const std::shared_ptr<NavigationTabs> tabs = createTabs();
    const std::shared_ptr<Router> router = getNavigation().createRouter({.routes = createRoutes(), .tabs = tabs, .schemes = {"music"}});
    frames(2);

    router->open("music://artist/42/album/7");
    frames(30);
    EXPECT_EQ(tabs->getSelected(), "search");
    EXPECT_EQ(describe(*tabs->getCurrent()), "search artist:\"42\" album:7");
    EXPECT_EQ(router->getLocation(*tabs->getCurrent()->getTop()), "artist/42/album/7");

    router->push("player");
    frames(30);
    ASSERT_EQ(getNavigation().getPresentations().size(), 1U);
    EXPECT_EQ(router->getLocation(*getNavigation().getPresentations().front()->getScreen()), "player");

    // A link replaces what the person was doing, so the presentation leaves.
    router->open("home");
    frames(30);
    EXPECT_EQ(tabs->getSelected(), "home");
    EXPECT_TRUE(getNavigation().getPresentations().empty());
    EXPECT_THROW(router->open("music://nowhere"), std::invalid_argument);
}

// A link that opens the app reaches the router through `linkOpened`, and a link that no route matches is told and leaves the app as it is.
TEST_F(RouterTest, OpensTheLinksThatOpenTheApp) {
    const std::shared_ptr<NavigationTabs> tabs = createTabs();
    const std::shared_ptr<Router> router = getNavigation().createRouter({.routes = createRoutes(), .tabs = tabs, .schemes = {"music"}});
    std::vector<std::string> missed;
    static_cast<void>(router->unmatched.connect([&missed](const std::string& url) { missed.push_back(url); }));

    getEngine().getEvents().emit(core::LifecycleEvent::kLinkOpened, {{"url", "music://artist/7"}});
    getEngine().getEvents().emit(core::LifecycleEvent::kLinkOpened, {{"url", "music://lost/7"}});
    frames(30);
    EXPECT_EQ(describe(*tabs->getCurrent()), "search artist:\"7\"");
    EXPECT_EQ(missed, (std::vector<std::string>{"music://lost/7"}));
}

// The link that launched the app arrives before the app creates its router, such as while its first scene loads, and the first router that opens links opens it once, after the state it restored.
TEST_F(RouterTest, OpensTheLinkThatLaunchedTheAppOnceTheRouterStarts) {
    getEngine().getEvents().emit(core::LifecycleEvent::kLinkOpened, {{"url", "music://lost/1"}});
    getEngine().getEvents().emit(core::LifecycleEvent::kLinkOpened, {{"url", "music://artist/12"}});
    frames(2);

    const std::shared_ptr<NavigationTabs> tabs = createTabs();
    const std::shared_ptr<Router> router = getNavigation().createRouter({.routes = createRoutes(), .tabs = tabs, .schemes = {"music"}});
    frames(30);
    EXPECT_EQ(tabs->getSelected(), "search");
    EXPECT_EQ(describe(*tabs->getCurrent()), "search artist:\"12\"");

    tabs->select("home");
    frames(30);
    const std::shared_ptr<Router> second = getNavigation().createRouter({.routes = createRoutes(), .tabs = createTabs(), .schemes = {"music"}});
    frames(30);
    EXPECT_EQ(tabs->getSelected(), "home");
}

// A route with a detail shows its screen as the detail of the split that the screen under it holds.
TEST_F(RouterTest, ShowsADetailInTheSplitOfTheScreenUnderIt) {
    const auto inbox = make("inbox");
    const std::shared_ptr<NavigationStack> stack = getNavigation().createStack({.gui = frame, .node = "middle"}, inbox);
    frames(2);
    const std::shared_ptr<NavigationSplit> split = getNavigation().createSplit({.gui = inbox->getGui(), .node = "list"}, {.gui = inbox->getGui(), .node = "detail"}, make("people"), {}, false);
    const std::shared_ptr<Router> router = getNavigation().createRouter({.routes = {{.name = "chat", .path = "chat/{person}", .detail = true, .screen = named("chat", "person")}}, .stack = stack});
    router->open("chat/ana");
    frames(30);
    EXPECT_EQ(describe(*split->getDetail()), "chat:\"ana\"");

    const std::shared_ptr<Router> lost = getNavigation().createRouter({.routes = {{.name = "chat", .path = "chat/{person}", .detail = true, .screen = named("chat", "person")}}, .stack = split->getDetail()});
    EXPECT_THROW(lost->push("chat/bruno"), std::logic_error);
}

// A detail restored in a tab that is not selected shows once the screen of the tab created its split, the first time the tab shows, and the router keeps it meanwhile.
TEST_F(RouterTest, RestoresADetailOnceTheSplitOfItsTabShows) {
    const auto mail = std::make_shared<SplitScreen>(getUi(), make("inbox"));
    // clang-format off
    const std::shared_ptr<NavigationTabs> tabs = getNavigation().createTabs({.gui = frame, .node = "middle"}, {
        {.id = "home", .root = [this] { return make("home"); }},
        {.id = "mail", .root = [mail] { return mail; }},
    }, "home");
    // clang-format on
    std::vector<Router::Route> routes{{.name = "chat", .path = "chat/{person}", .tab = "mail", .detail = true, .screen = named("chat", "person")}};
    const std::shared_ptr<Router> router = getNavigation().createRouter({.routes = std::move(routes), .tabs = tabs});
    frames(2);

    router->restore({{"tab", "home"}, {"details", {"chat/ana"}}});
    frames(2);
    EXPECT_EQ(tabs->getSelected(), "home");
    EXPECT_EQ(mail->split, nullptr);
    EXPECT_EQ(router->save().at("details"), (core::Json{"chat/ana"}));

    tabs->select("mail");
    frames(30);
    ASSERT_NE(mail->split, nullptr);
    EXPECT_EQ(describe(*mail->split->getDetail()), "chat:\"ana\"");
    EXPECT_EQ(router->save().at("details"), (core::Json{"chat/ana"}));
}

// The state of the navigation reads back as the same tabs, stacks and presentations, made again from their locations.
TEST_F(RouterTest, SavesAndRestoresTheStateOfTheNavigation) {
    const std::shared_ptr<NavigationTabs> tabs = createTabs();
    const std::shared_ptr<Router> router = getNavigation().createRouter({.routes = createRoutes(), .tabs = tabs});
    router->open("artist/42/album/7?track=3");
    router->push("player");
    frames(30);
    const core::Json state = router->save();
    EXPECT_EQ(state.at("tab"), "search");
    EXPECT_EQ(state.at("stacks").at("search"), (core::Json{"artist/42", "artist/42/album/7?track=3"}));
    EXPECT_EQ(state.at("presented"), (core::Json{"player"}));

    router->open("home");
    frames(30);
    core::Json changed = state;
    changed["stacks"]["search"].push_back("lost/1");
    router->restore(changed);
    frames(30);
    EXPECT_EQ(tabs->getSelected(), "search");
    EXPECT_EQ(describe(*tabs->getCurrent()), "search artist:\"42\" album:7");
    EXPECT_EQ(getNavigation().getPresentations().size(), 1U);
}

// Routes of Lua open links with typed parameters through the tabs and the screens of their parents, push a location, match a link and locate a screen, tell about a link that no route matches, and keep the state of the navigation across a restart.
TEST_F(RouterTest, RoutesLinksFromLuaAndKeepsTheStateAcrossARestart) {
    // clang-format off
    const std::string app = R"(
        local haylen = require('haylen')
        ui = require('haylen.ui')
        Page = haylen.class('Page', ui.Screen)
        function Page:init(name) self.name = name end
        function Page:build() return ui.column{ui.label{text = self.name}} end
        frame = ui.mount(ui.column{ui.column{id = 'middle', grow = 1, align = 'stretch'}}, {placement = 'screen'})
        tabs = ui.navigationTabs{gui = frame, id = 'middle', tabs = {{id = 'home', root = function() return Page('home') end}, {id = 'search', root = function() return Page('search') end}}}
        missed = {}
        router = ui.router{tabs = tabs, schemes = {'music'}, restore = true, onUnmatched = function(event) missed[#missed + 1] = event.url end, routes = {
            {name = 'home', path = 'home', tab = 'home'},
            {name = 'artist', path = 'artist/{artist}', tab = 'search', screen = function(p) return Page('artist ' .. p.artist) end},
            {name = 'album', path = 'artist/{artist}/album/{id:integer}', query = {track = 'integer'}, parent = 'artist', screen = function(p) return Page('album ' .. p.id .. ' ' .. tostring(p.track)) end},
            {name = 'player', path = 'player', present = {style = 'fullScreen'}, screen = function() return Page('player') end},
        }}
        function names()
            local list = {}
            for index, screen in ipairs(tabs.current.screens) do list[index] = screen.name end
            return tabs.selected .. ': ' .. table.concat(list, ', ')
        end
    )";
    // clang-format on
    getFixture().runLua(app);
    getFixture().runLua("router:open('music://artist/sea/album/7?track=2')");
    getEngine().getEvents().emit(core::LifecycleEvent::kLinkOpened, {{"url", "music://lost/1"}});
    frames(30);
    EXPECT_EQ(getFixture().lua("return names()"), "search: search, artist sea, album 7 2");
    EXPECT_EQ(getFixture().lua("return router:location(tabs.current.top) .. ' ' .. table.concat(missed, ',')"), "artist/sea/album/7?track=2 music://lost/1");
    EXPECT_EQ(getFixture().lua("local name, p = router:match('music://artist/sea/album/7') return name .. ' ' .. p.artist .. ' ' .. p.id"), "album sea 7");
    EXPECT_EQ(getFixture().lua("return tostring(router:match('music://lost'))"), "nil");
    EXPECT_NE(getFixture().lua("router:open('lost/1')").find("No route matches the link \"lost/1\"."), std::string::npos);
    EXPECT_NE(getFixture().lua("router:match('artist/a/album/7?track=one')").find("The query parameter \"track\" of the link \"artist/a/album/7?track=one\" must be of the type \"integer\"."), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.router{tabs = tabs, routes = {{name = 'a', path = 'a', speed = 2}}}").find("Unknown option \"speed\""), std::string::npos);

    // A restart, such as after a change of the code in development, builds the same state again from the locations the router kept.
    getFixture().runLua("router:push('player')");
    frames(30);
    getEngine().requestRestart();
    getFixture().restart();
    getFixture().runLua(app);
    frames(30);
    EXPECT_EQ(getFixture().lua("return names()"), "search: search, artist sea, album 7 2");
    EXPECT_EQ(getUi().getNavigation().getPresentations().size(), 1U);
}

// A link that no route reads, such as one whose query does not read as its type, is told like a link that no route matches, and the links after it still open.
TEST_F(RouterTest, TellsALinkThatNoRouteReadsAndOpensTheNextOne) {
    const std::shared_ptr<NavigationTabs> tabs = createTabs();
    const std::shared_ptr<Router> router = getNavigation().createRouter({.routes = createRoutes(), .tabs = tabs, .schemes = {"music"}});
    std::vector<std::string> missed;
    static_cast<void>(router->unmatched.connect([&missed](const std::string& url) { missed.push_back(url); }));

    getEngine().getEvents().emit(core::LifecycleEvent::kLinkOpened, {{"url", "music://artist/7/album/1?track=first"}});
    getEngine().getEvents().emit(core::LifecycleEvent::kLinkOpened, {{"url", "music://artist/7"}});
    frames(30);
    EXPECT_EQ(missed, (std::vector<std::string>{"music://artist/7/album/1?track=first"}));
    EXPECT_EQ(describe(*tabs->getCurrent()), "search artist:\"7\"");
}

// The state that a router keeps survives one owner that ends its tabs before it, as a scene does when the app stops, and a router whose state fails to build closes and leaves the links to the next one.
TEST_F(RouterTest, KeepsTheStateWhenItsTabsCloseFirstAndClosesWhenItFailsToBuildIt) {
    std::shared_ptr<NavigationTabs> tabs = createTabs();
    std::shared_ptr<Router> router = getNavigation().createRouter({.routes = createRoutes(), .tabs = tabs, .restores = true});
    router->open("artist/42");
    frames(30);
    // clang-format off
    core::ScopedConnection ending = getEngine().getEvents().on(core::LifecycleEvent::kAppStopping, [&](core::EventBus::Event&) {
        tabs->close();
        router->close();
    });
    // clang-format on
    getEngine().requestRestart();
    getFixture().restart();
    ending.disconnect();

    frame = mount(R"({"kind": "column", "children": [{"kind": "column", "id": "middle", "grow": 1, "align": "stretch"}]})");
    tabs = createTabs();
    router = getNavigation().createRouter({.routes = createRoutes(), .tabs = tabs, .restores = true});
    frames(30);
    EXPECT_EQ(describe(*tabs->getCurrent()), "search artist:\"42\"");
    router->close();
    tabs->close();

    getEngine().requestRestart();
    getFixture().restart();
    frame = mount(R"({"kind": "column", "children": [{"kind": "column", "id": "middle", "grow": 1, "align": "stretch"}]})");
    tabs = createTabs();
    std::vector<Router::Route> failing = createRoutes();
    failing[1].screen = [](const core::Json&) -> std::shared_ptr<Screen> { throw std::runtime_error("The artist could not load."); };
    EXPECT_THROW(static_cast<void>(getNavigation().createRouter({.routes = std::move(failing), .tabs = tabs, .schemes = {"music"}, .restores = true})), std::runtime_error);

    getEngine().getEvents().emit(core::LifecycleEvent::kLinkOpened, {{"url", "music://artist/9"}});
    frames(2);
    EXPECT_EQ(tabs->getSelected(), "home");
    router = getNavigation().createRouter({.routes = createRoutes(), .tabs = tabs, .schemes = {"music"}});
    frames(30);
    EXPECT_EQ(describe(*tabs->getCurrent()), "search artist:\"9\"");
}

// A plus sign stands for a space only in a query, so a segment of a path keeps it.
TEST_F(RouterTest, ReadsAPlusSignAsASpaceOnlyInTheQuery) {
    const std::shared_ptr<NavigationStack> stack = getNavigation().createStack({.gui = frame, .node = "middle"}, make("root"));
    const std::shared_ptr<Router> router = getNavigation().createRouter({.routes = {{.name = "search", .path = "search/{term}", .query = {{"q", Router::Type::String}}}}, .stack = stack});
    const std::optional<Router::Match> search = router->match("search/rock+roll?q=rock+roll");
    ASSERT_TRUE(search.has_value());
    EXPECT_EQ(search->parameters, (core::Json{{"term", "rock+roll"}, {"q", "rock roll"}}));
}

// A saved location whose query no longer reads as its type is left out, and the rest of the state builds again.
TEST_F(RouterTest, LeavesOutASavedLocationThatNoRouteReads) {
    const std::shared_ptr<NavigationTabs> tabs = createTabs();
    const std::shared_ptr<Router> router = getNavigation().createRouter({.routes = createRoutes(), .tabs = tabs});
    frames(2);
    router->restore({{"tab", "search"}, {"stacks", {{"search", {"artist/42", "artist/42/album/7?track=first"}}}}});
    frames(30);
    EXPECT_EQ(tabs->getSelected(), "search");
    EXPECT_EQ(describe(*tabs->getCurrent()), "search artist:\"42\"");
}

// A router whose tabs closed shows nothing, so the links, the one that launched the app among them, go to a router that still shows.
TEST_F(RouterTest, LeavesLinksToTheRoutersThatStillShow) {
    getEngine().getEvents().emit(core::LifecycleEvent::kLinkOpened, {{"url", "music://artist/12"}});
    frames(2);
    const std::shared_ptr<NavigationTabs> gone = createTabs();
    const std::shared_ptr<Router> stale = getNavigation().createRouter({.routes = createRoutes(), .tabs = gone, .schemes = {"music"}});
    gone->close();

    const std::shared_ptr<NavigationTabs> tabs = createTabs();
    const std::shared_ptr<Router> router = getNavigation().createRouter({.routes = createRoutes(), .tabs = tabs, .schemes = {"music"}});
    frames(30);
    EXPECT_EQ(describe(*tabs->getCurrent()), "search artist:\"12\"");

    getEngine().getEvents().emit(core::LifecycleEvent::kLinkOpened, {{"url", "music://artist/7"}});
    frames(30);
    EXPECT_EQ(describe(*tabs->getCurrent()), "search artist:\"7\"");
    EXPECT_EQ(getEngine().getError(), nullptr);
}

TEST_F(RouterTest, RefusesRoutesThatCannotWork) {
    const std::shared_ptr<NavigationTabs> tabs = createTabs();
    EXPECT_THROW(static_cast<void>(getNavigation().createRouter({.routes = createRoutes()})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(getNavigation().createRouter({.routes = {{.name = "a", .path = "a"}, {.name = "a", .path = "b"}}, .tabs = tabs})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(getNavigation().createRouter({.routes = {{.name = "a", .path = "a", .tab = "lost"}}, .tabs = tabs})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(getNavigation().createRouter({.routes = {{.name = "a", .path = "a", .parent = "b"}, {.name = "b", .path = "b", .parent = "a"}}, .tabs = tabs})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(getNavigation().createRouter({.routes = {{.name = "a", .path = "a/{id:color}"}}, .tabs = tabs})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(getNavigation().createRouter({.routes = {{.name = "a", .path = "a", .detail = true, .presentation = Presentation::Options{}}}, .tabs = tabs})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(getNavigation().createRouter({.routes = {{.name = "artist", .path = "artist/{artist}"}, {.name = "album", .path = "album/{id}", .parent = "artist"}}, .tabs = tabs})), std::invalid_argument);
}

} // namespace haylen::ui
