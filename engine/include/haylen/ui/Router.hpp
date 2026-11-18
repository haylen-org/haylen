#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/core/ScopedConnection.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/ui/NavigationTransition.hpp"
#include "haylen/ui/Presentation.hpp"

namespace haylen::core {
class Engine;
}

namespace haylen::ui {

class Navigation;
class NavigationStack;
class NavigationTabs;
class Screen;

// The routes of an app, which turn a link or a location into the state of its navigation: the selected tab, the screens of the stack of that tab under the screen of the route, the detail of a split and the presentations. So a link that opens the app, or arrives while it runs, lands on the right screen with the screens the person would have reached by hand under it. Every route has a path of segments, some of which are typed parameters such as `artist/{id:integer}`, and typed query parameters. A screen that the router shows keeps its location, so the router saves the state of the navigation as locations and builds it again when the app starts after a restart or after the system ended it in the background.
class Router final : public std::enable_shared_from_this<Router> {
  public:
    // The type of a parameter, which its text in a link must read as.
    enum class Type : std::uint8_t {
        String,
        Integer,
        Number,
        Boolean,
    };

    using Factory = std::function<std::shared_ptr<Screen>(const core::Json& parameters)>;

    struct Route {
        // The name of the route, which another route names as its parent.
        std::string name;

        // The segments of the path, each a literal or a parameter such as `{id}` or `{id:integer}`.
        std::string path;

        // The query parameters the route reads, which a link may leave out.
        std::vector<std::pair<std::string, Type>> query;

        // The tab the route shows in, when the router has tabs. Without one, it shows in the tab of its nearest parent that names one, or in the selected tab.
        std::string tab;

        // The route whose screens lie under the screen of this one, made from the same parameters.
        std::string parent;

        // Shows the screen as the detail of the split that the screen under it holds.
        bool detail = false;

        // Presents the screen over everything with these options instead of showing it in a stack.
        std::optional<Presentation::Options> presentation;

        // Makes the screen of the route from its parameters. A route without one only selects its tab and shows the root of the tab.
        Factory screen;
    };

    struct Options {
        std::vector<Route> routes;

        // Where the routes show their screens: the stacks of the tabs, or one stack.
        std::shared_ptr<NavigationTabs> tabs;
        std::shared_ptr<NavigationStack> stack;

        // The schemes of the links of the app, such as `music`, whose host is the first segment of their path, and the hosts of its web links, such as `music.example.com`. A link of another scheme or host matches no route.
        std::vector<std::string> schemes;
        std::vector<std::string> hosts;

        // Whether the router opens the links that open the app or arrive while it runs, which `linkOpened` reports.
        bool links = true;

        // Whether the router keeps the state of the navigation and builds it again when the app starts after a restart or after the system ended it in the background.
        bool restores = false;
    };

    // What a link matched: the route and its typed parameters.
    struct Match {
        const Route* route = nullptr;
        core::Json parameters = core::Json::object();
    };

    // Navigation creates routers. Throws `std::invalid_argument` for no tabs and no stack or both, a route without a name or a path, a repeated name, a route that is both a detail and a presentation, a parent that no route has, that leads back to the route, that is not a route of a stack or whose path has a parameter that the path of the route lacks, a path with an unknown type or an empty parameter, and a tab that the tabs lack.
    Router(Navigation& owner, core::Engine& running, Options settings);
    ~Router();

    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

    // Returns the route that a link or a location matches with its parameters, or nothing. Throws `std::invalid_argument` for a query parameter whose text does not read as its type.
    [[nodiscard]] std::optional<Match> match(std::string_view link) const;

    // Builds the state that a link or a location opens: dismisses the modal presentations, selects the tab of its route, sets the stack of the tab to its root and the screens of the route and its parents, and shows the detail or presents the screen of a route that does. Throws `std::invalid_argument` for a link that no route matches.
    void open(std::string_view link, const NavigationTransition& transition = {});

    // Shows the screen of a location in the flow that shows: pushes it on the stack of the selected tab, shows it as a detail or presents it. Throws `std::invalid_argument` for a location that no route matches.
    void push(std::string_view location, const NavigationTransition& transition = {});

    // The location that the router showed a screen for, or an empty text for a screen it did not show.
    [[nodiscard]] std::string getLocation(const Screen& screen) const;

    // Returns the state of the navigation, the selected tab with the locations of the screens of every stack, of the details and of the presentations, which `restore` builds again. Locations that no route matches anymore are left out.
    [[nodiscard]] core::Json save() const;
    void restore(const core::Json& state);

    // Stops opening links and keeping the state.
    void close();
    [[nodiscard]] bool isClosed() const noexcept {
        return closed;
    }

    // Tells about a link that no route matches, which the router logs as an error.
    core::Signal<const std::string&> unmatched;

    static constexpr std::array<std::pair<std::string_view, Type>, 4> kTypeNames{{{"string", Type::String}, {"integer", Type::Integer}, {"number", Type::Number}, {"boolean", Type::Boolean}}};

  private:
    friend class Navigation;

    struct Segment {
        std::string text;
        bool parameter = false;
        Type type = Type::String;
    };

    // A link split into the segments of its path and the pairs of its query, decoded.
    struct Address {
        std::vector<std::string> segments;
        std::vector<std::pair<std::string, std::string>> query;
    };

    // A screen that the router showed and its location.
    struct Shown {
        std::weak_ptr<Screen> screen;
        std::string location;
    };

    [[nodiscard]] static std::vector<Segment> compile(const Route& route);
    [[nodiscard]] static std::string decode(std::string_view text, bool query);
    [[nodiscard]] static std::string encode(std::string_view text);
    [[nodiscard]] static std::optional<core::Json> read(std::string_view text, Type type);
    [[nodiscard]] std::optional<Address> parse(std::string_view link) const;
    [[nodiscard]] const Route& findRoute(std::string_view name) const;

    // The location of a route with its parameters, which matches the route again.
    [[nodiscard]] std::string locate(const Route& route, const core::Json& parameters) const;
    [[nodiscard]] Match require(std::string_view link) const;

    // Makes the screen of a route and keeps its location.
    [[nodiscard]] std::shared_ptr<Screen> make(const Route& route, const core::Json& parameters);

    // The tab of a route or of its nearest parent that names one, or an empty text.
    [[nodiscard]] const std::string& getTab(const Route& route) const;

    // The stack a route shows in: the stack of its tab or of the selected tab, or the one stack of the router.
    [[nodiscard]] std::shared_ptr<NavigationStack> getStack(const Route& route) const;
    void select(const Route& route) const;
    void showDetail(const Route& route, const std::shared_ptr<Screen>& screen, const NavigationTransition& transition) const;
    void present(const Route& route, std::shared_ptr<Screen> screen) const;

    // Whether the tabs or the stack where the routes show are still open.
    [[nodiscard]] bool isShowing() const noexcept;

    // Keeps the state of the navigation for the next start, as the app goes to the background or stops.
    void persist() const;

    // Opens a link that opened the app, or logs and tells that no route matches it.
    void follow(const std::string& url);

    Navigation& navigation;
    core::Engine& engine;
    Options options;
    std::vector<std::vector<Segment>> patterns;
    std::vector<Shown> shown;
    core::ScopedConnection linkConnection;
    core::ScopedConnection backgroundConnection;
    core::ScopedConnection stoppingConnection;
    bool closed = false;
};

} // namespace haylen::ui
