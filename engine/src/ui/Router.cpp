#include "haylen/ui/Router.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/plugins/StoragePlugin.hpp"
#include "haylen/storage/ResumeState.hpp"
#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/NavigationSplit.hpp"
#include "haylen/ui/NavigationStack.hpp"
#include "haylen/ui/NavigationTabs.hpp"
#include "haylen/ui/Screen.hpp"

namespace haylen::ui {

Router::Router(Navigation& owner, core::Engine& running, Options settings) : navigation(owner), engine(running), options(std::move(settings)) {
    if (!options.tabs == !options.stack) {
        throw std::invalid_argument("A router shows its routes in the stacks of \"tabs\" or in one \"stack\".");
    }

    std::set<std::string, std::less<>> names;
    for (const Route& route : options.routes) {
        if (route.name.empty()) {
            throw std::invalid_argument("Every route needs a name.");
        }
        if (!names.insert(route.name).second) {
            throw std::invalid_argument("The route \"" + route.name + "\" is given more than once.");
        }
        if (!route.tab.empty() && (!options.tabs || !options.tabs->hasTab(route.tab))) {
            throw std::invalid_argument("The route \"" + route.name + "\" shows in the tab \"" + route.tab + "\", which the router has no tabs with.");
        }

        patterns.push_back(compile(route));
    }

    // Every parent is a route of a stack, and following the parents never comes back to a route.
    for (const Route& route : options.routes) {
        std::set<std::string, std::less<>> visited{route.name};
        for (const Route* step = &route; !step->parent.empty();) {
            const auto parent = std::ranges::find(options.routes, step->parent, &Route::name);
            if (parent == options.routes.end()) {
                throw std::invalid_argument("The route \"" + step->name + "\" names the parent \"" + step->parent + "\", which no route has.");
            }
            if (parent->detail || parent->presentation) {
                throw std::invalid_argument("The parent of a route is a route of a stack, which \"" + parent->name + "\" is not.");
            }
            if (!visited.insert(parent->name).second) {
                throw std::invalid_argument("The parents of the route \"" + route.name + "\" lead back to a route they passed.");
            }

            step = &*parent;
        }
    }

    // clang-format off
    if (options.links) {
        linkConnection = engine.getEvents().on(core::LifecycleEvent::kLinkOpened, [this](core::EventBus::Event& event) {
            follow(event.getData().value("url", std::string()));
        });
    }

    if (options.restores) {
        backgroundConnection = engine.getEvents().on(core::LifecycleEvent::kAppBackground, [this](core::EventBus::Event&) {
            persist();
        });
    }
    // clang-format on
}

Router::~Router() = default;

void Router::follow(const std::string& url) {
    if (!match(url)) {
        core::Log::error("No route matches the link \"{}\".", url);
        unmatched.emit(url);
        return;
    }

    open(url);
}

// A segment in braces is a parameter, `{name}` or `{name:type}`, and any other segment is a literal.
std::vector<Router::Segment> Router::compile(const Route& route) {
    std::vector<Segment> segments;
    std::string_view rest = route.path;
    while (!rest.empty()) {
        const std::size_t slash = rest.find('/');
        const std::string_view part = rest.substr(0, slash);
        rest = slash == std::string_view::npos ? std::string_view{} : rest.substr(slash + 1);
        if (part.empty()) {
            continue;
        }

        if (part.front() != '{' || part.back() != '}') {
            segments.push_back({.text = std::string(part)});
            continue;
        }

        const std::string_view inside = part.substr(1, part.size() - 2);
        const std::size_t colon = inside.find(':');
        const std::string_view name = inside.substr(0, colon);
        Segment segment{.text = std::string(name), .parameter = true};
        if (colon != std::string_view::npos) {
            const std::string_view type = inside.substr(colon + 1);
            const auto found = std::ranges::find(kTypeNames, type, &std::pair<std::string_view, Type>::first);
            if (found == kTypeNames.end()) {
                throw std::invalid_argument("The path of the route \"" + route.name + "\" has a parameter of the type \"" + std::string(type) + "\", which is not \"string\", \"integer\", \"number\" or \"boolean\".");
            }

            segment.type = found->second;
        }

        if (name.empty()) {
            throw std::invalid_argument("The path of the route \"" + route.name + "\" has a parameter without a name.");
        }

        segments.push_back(std::move(segment));
    }

    return segments;
}

std::string Router::decode(std::string_view text) {
    std::string decoded;
    decoded.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char character = text[index];
        if (character == '+') {
            decoded.push_back(' ');
            continue;
        }

        int value = 0;
        const bool escaped = character == '%' && index + 2 < text.size() && std::from_chars(text.data() + index + 1, text.data() + index + 3, value, 16).ptr == text.data() + index + 3;
        if (escaped) {
            decoded.push_back(static_cast<char>(value));
            index += 2;
            continue;
        }

        decoded.push_back(character);
    }

    return decoded;
}

// The letters, the digits and a few marks stay as they are, and every other byte is escaped, so the location reads back the same.
std::string Router::encode(std::string_view text) {
    constexpr std::string_view kHex = "0123456789ABCDEF";
    std::string encoded;
    for (const char character : text) {
        const auto byte = static_cast<unsigned char>(character);
        const bool plain = (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' || byte == '.' || byte == '~';
        if (plain) {
            encoded.push_back(character);
            continue;
        }

        encoded.push_back('%');
        encoded.push_back(kHex[byte >> 4U]);
        encoded.push_back(kHex[byte & 0x0FU]);
    }

    return encoded;
}

std::optional<core::Json> Router::read(std::string_view text, Type type) {
    switch (type) {
    case Type::String:
        return core::Json(std::string(text));
    case Type::Integer: {
        std::int64_t value = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        return error == std::errc{} && end == text.data() + text.size() && !text.empty() ? std::optional(core::Json(value)) : std::nullopt;
    }
    case Type::Number: {
        // A number reads the way JSON writes one, whatever the locale of the system.
        const bool spaced = text.find_first_of(" \t\r\n") != std::string_view::npos;
        const core::Json value = spaced ? core::Json() : core::Json::parse(text, nullptr, false);
        return value.is_number() && std::isfinite(value.get<double>()) ? std::optional(core::Json(value.get<double>())) : std::nullopt;
    }
    case Type::Boolean:
        if (text == "true" || text == "false") {
            return core::Json(text == "true");
        }

        return std::nullopt;
    }

    return std::nullopt;
}

// A link of a scheme of the app reads its host as the first segment, a web link of a host of the app reads only its path, and a location reads as a path. The fragment never takes part.
std::optional<Router::Address> Router::parse(std::string_view link) const {
    link = link.substr(0, link.find('#'));
    const std::size_t mark = link.find('?');
    const std::string_view query = mark == std::string_view::npos ? std::string_view{} : link.substr(mark + 1);
    std::string_view path = link.substr(0, mark);

    Address address;
    const std::size_t separator = path.find("://");
    if (separator != std::string_view::npos) {
        const std::string_view scheme = path.substr(0, separator);
        const std::string_view rest = path.substr(separator + 3);
        const std::size_t slash = rest.find('/');
        const std::string_view host = rest.substr(0, slash);
        const bool ownScheme = std::ranges::find(options.schemes, scheme) != options.schemes.end();
        const bool ownHost = std::ranges::find(options.hosts, host) != options.hosts.end();
        if (!ownScheme && !ownHost) {
            return std::nullopt;
        }

        path = ownScheme ? rest : (slash == std::string_view::npos ? std::string_view{} : rest.substr(slash));
    }

    while (!path.empty()) {
        const std::size_t slash = path.find('/');
        const std::string_view part = path.substr(0, slash);
        path = slash == std::string_view::npos ? std::string_view{} : path.substr(slash + 1);
        if (!part.empty()) {
            address.segments.push_back(decode(part));
        }
    }

    std::string_view pairs = query;
    while (!pairs.empty()) {
        const std::size_t ampersand = pairs.find('&');
        const std::string_view pair = pairs.substr(0, ampersand);
        pairs = ampersand == std::string_view::npos ? std::string_view{} : pairs.substr(ampersand + 1);
        if (pair.empty()) {
            continue;
        }

        const std::size_t equals = pair.find('=');
        const std::string_view value = equals == std::string_view::npos ? std::string_view{} : pair.substr(equals + 1);
        address.query.emplace_back(decode(pair.substr(0, equals)), decode(value));
    }

    return address;
}

// The routes match in the order the router lists them, and a segment whose text does not read as the type of its parameter makes the route not match. The query parameters a route does not read stay out of its parameters.
std::optional<Router::Match> Router::match(std::string_view link) const {
    const std::optional<Address> address = parse(link);
    if (!address) {
        return std::nullopt;
    }

    for (std::size_t index = 0; index < options.routes.size(); ++index) {
        const std::vector<Segment>& segments = patterns[index];
        if (segments.size() != address->segments.size()) {
            continue;
        }

        Match found{.route = &options.routes[index]};
        bool matches = true;
        for (std::size_t position = 0; position < segments.size() && matches; ++position) {
            const Segment& segment = segments[position];
            const std::string& text = address->segments[position];
            if (!segment.parameter) {
                matches = segment.text == text;
                continue;
            }

            std::optional<core::Json> value = read(text, segment.type);
            matches = value.has_value();
            if (matches) {
                found.parameters[segment.text] = std::move(*value);
            }
        }

        if (!matches) {
            continue;
        }

        for (const auto& [name, type] : found.route->query) {
            const auto given = std::ranges::find(address->query, name, &std::pair<std::string, std::string>::first);
            if (given == address->query.end()) {
                continue;
            }

            std::optional<core::Json> value = read(given->second, type);
            if (!value) {
                const auto typeName = std::ranges::find(kTypeNames, type, &std::pair<std::string_view, Type>::second)->first;
                throw std::invalid_argument("The query parameter \"" + name + "\" of the link \"" + std::string(link) + "\" must be of the type \"" + std::string(typeName) + "\".");
            }

            found.parameters[name] = std::move(*value);
        }

        return found;
    }

    return std::nullopt;
}

Router::Match Router::require(std::string_view link) const {
    if (closed) {
        throw std::logic_error("The router is closed.");
    }

    std::optional<Match> found = match(link);
    if (!found) {
        throw std::invalid_argument("No route matches the link \"" + std::string(link) + "\".");
    }

    return std::move(*found);
}

const Router::Route& Router::findRoute(std::string_view name) const {
    return *std::ranges::find(options.routes, name, &Route::name);
}

std::string Router::locate(const Route& route, const core::Json& parameters) const {
    // clang-format off
    const auto text = [](const core::Json& value) {
        return value.is_string() ? value.get<std::string>() : value.dump();
    };
    // clang-format on
    const std::vector<Segment>& segments = patterns[static_cast<std::size_t>(&route - options.routes.data())];
    std::string location;
    for (const Segment& segment : segments) {
        location += location.empty() ? "" : "/";
        location += segment.parameter ? encode(text(parameters.at(segment.text))) : encode(segment.text);
    }

    char separator = '?';
    for (const auto& [name, type] : route.query) {
        if (!parameters.contains(name)) {
            continue;
        }

        location += separator;
        location += encode(name) + "=" + encode(text(parameters.at(name)));
        separator = '&';
    }

    return location;
}

std::shared_ptr<Screen> Router::make(const Route& route, const core::Json& parameters) {
    std::shared_ptr<Screen> screen = route.screen(parameters);
    if (!screen) {
        throw std::logic_error("The route \"" + route.name + "\" made no screen.");
    }

    std::erase_if(shown, [](const Shown& entry) { return entry.screen.expired(); });
    shown.push_back({.screen = screen, .location = locate(route, parameters)});
    return screen;
}

// A route without a tab shows in the tab of its nearest parent that names one.
const std::string& Router::getTab(const Route& route) const {
    const Route* step = &route;
    while (step->tab.empty() && !step->parent.empty()) {
        step = &findRoute(step->parent);
    }

    return step->tab;
}

std::shared_ptr<NavigationStack> Router::getStack(const Route& route) const {
    if (!options.tabs) {
        return options.stack;
    }

    const std::string& tab = getTab(route);
    return options.tabs->getStack(tab.empty() ? options.tabs->getSelected() : tab);
}

void Router::select(const Route& route) const {
    const std::string& tab = options.tabs ? getTab(route) : route.tab;
    if (options.tabs && !tab.empty() && options.tabs->getSelected() != tab) {
        options.tabs->select(tab);
    }
}

void Router::showDetail(const Route& route, const std::shared_ptr<Screen>& screen, const NavigationTransition& transition) const {
    const std::shared_ptr<NavigationStack> stack = getStack(route);
    const std::shared_ptr<Screen> host = stack->getTop();
    const std::shared_ptr<NavigationSplit> split = host ? navigation.findSplit(*host) : nullptr;
    if (!split) {
        throw std::logic_error("The route \"" + route.name + "\" shows a detail, but the screen under it holds no split.");
    }

    split->showDetail(screen, transition.isInstant() ? std::nullopt : std::optional(transition));
}

void Router::present(const Route& route, std::shared_ptr<Screen> screen) const {
    static_cast<void>(navigation.present(std::move(screen), *route.presentation));
}

// A link replaces what the person was doing: the modal presentations leave, and the stack of the route holds its root, the screens of its parents and its own screen, or the screen under its detail or its presentation.
void Router::open(std::string_view link, const NavigationTransition& transition) {
    const Match found = require(link);
    const Route& route = *found.route;
    const std::vector<std::shared_ptr<Presentation>> presented = navigation.getPresentations();
    for (const std::shared_ptr<Presentation>& presentation : presented) {
        if (presentation->isModal()) {
            presentation->dismiss(nullptr, NavigationTransition{.effect = NavigationTransition::Effect::None});
        }
    }

    std::vector<const Route*> chain{&route};
    while (!chain.front()->parent.empty()) {
        chain.insert(chain.begin(), &findRoute(chain.front()->parent));
    }

    select(route);
    const std::shared_ptr<NavigationStack> stack = getStack(route);
    std::vector<std::shared_ptr<Screen>> screens;
    if (!stack->empty()) {
        screens.push_back(stack->getScreens().front());
    }

    const bool aside = route.detail || route.presentation;
    for (const Route* step : chain) {
        if (step->screen && !(step == &route && aside)) {
            screens.push_back(make(*step, found.parameters));
        }
    }

    stack->setScreens(std::move(screens), transition);
    if (!route.screen || !aside) {
        return;
    }

    std::shared_ptr<Screen> screen = make(route, found.parameters);
    if (route.detail) {
        showDetail(route, screen, transition);
        return;
    }

    present(route, std::move(screen));
}

void Router::push(std::string_view location, const NavigationTransition& transition) {
    const Match found = require(location);
    const Route& route = *found.route;
    if (!route.screen) {
        select(route);
        getStack(route)->popToRoot(transition);
        return;
    }

    std::shared_ptr<Screen> screen = make(route, found.parameters);
    if (route.presentation) {
        present(route, std::move(screen));
        return;
    }

    if (route.detail) {
        showDetail(route, screen, transition);
        return;
    }

    const std::shared_ptr<NavigationStack> stack = options.tabs ? options.tabs->getCurrent() : options.stack;
    stack->push(std::move(screen), transition);
}

std::string Router::getLocation(const Screen& screen) const {
    const auto found = std::ranges::find_if(shown, [&screen](const Shown& entry) { return entry.screen.lock().get() == &screen; });
    return found != shown.end() ? found->location : std::string();
}

// The screens of a stack keep their order, and the details and the presentations follow in the order they showed.
core::Json Router::save() const {
    core::Json stacks = core::Json::object();
    // clang-format off
    const auto record = [&](const std::string& key, const std::shared_ptr<NavigationStack>& stack) {
        core::Json locations = core::Json::array();
        for (const std::shared_ptr<Screen>& screen : stack->getScreens()) {
            const std::string location = getLocation(*screen);
            if (!location.empty()) {
                locations.push_back(location);
            }
        }

        stacks[key] = std::move(locations);
    };
    // clang-format on
    if (options.tabs) {
        for (const std::string& id : options.tabs->getTabIds()) {
            if (const std::shared_ptr<NavigationStack> stack = options.tabs->findStack(id)) {
                record(id, stack);
            }
        }
    } else {
        record("", options.stack);
    }

    // A detail stays a detail whether its split shows it beside the list or on top of it.
    core::Json details = core::Json::array();
    core::Json presented = core::Json::array();
    for (const Shown& entry : shown) {
        const std::shared_ptr<Screen> screen = entry.screen.lock();
        if (!screen || screen->getState() == Screen::State::Removed) {
            continue;
        }

        const std::optional<Match> found = match(entry.location);
        if (screen->getPresentation()) {
            presented.push_back(entry.location);
        } else if (found && found->route->detail) {
            details.push_back(entry.location);
        }
    }

    core::Json state = {{"stacks", std::move(stacks)}, {"details", std::move(details)}, {"presented", std::move(presented)}};
    if (options.tabs) {
        state["tab"] = options.tabs->getSelected();
    }

    return state;
}

// A saved location that no route matches anymore, such as after an update of the app, is left out with a warning.
void Router::restore(const core::Json& state) {
    if (closed) {
        throw std::logic_error("The router is closed.");
    }
    if (!state.is_object()) {
        throw std::invalid_argument("The state of a router is an object that `save` returned.");
    }

    // clang-format off
    const auto locations = [&state](const char* key) {
        const auto found = state.find(key);
        return found != state.end() && found->is_array() ? *found : core::Json::array();
    };
    const auto find = [this](const core::Json& location) -> std::optional<Match> {
        std::optional<Match> found = location.is_string() ? match(location.get<std::string>()) : std::nullopt;
        if (!found) {
            core::Log::warning("The saved location {} matches no route, so the navigation starts without it.", location.dump());
        }
        return found;
    };
    // clang-format on
    const NavigationTransition instant{.effect = NavigationTransition::Effect::None};
    const auto stacks = state.find("stacks");
    if (stacks != state.end() && stacks->is_object()) {
        for (const auto& [key, list] : stacks->items()) {
            if (options.tabs && !options.tabs->hasTab(key)) {
                continue;
            }

            const std::shared_ptr<NavigationStack> stack = options.tabs ? options.tabs->getStack(key) : options.stack;
            std::vector<std::shared_ptr<Screen>> screens;
            if (!stack->empty()) {
                screens.push_back(stack->getScreens().front());
            }

            for (const core::Json& location : list.is_array() ? list : core::Json::array()) {
                if (const std::optional<Match> found = find(location); found && found->route->screen && !found->route->detail && !found->route->presentation) {
                    screens.push_back(make(*found->route, found->parameters));
                }
            }

            stack->setScreens(std::move(screens), instant);
        }
    }

    const auto tab = state.find("tab");
    if (options.tabs && tab != state.end() && tab->is_string() && options.tabs->hasTab(tab->get<std::string>()) && options.tabs->getSelected() != tab->get<std::string>()) {
        options.tabs->select(tab->get<std::string>());
    }

    for (const char* key : {"details", "presented"}) {
        for (const core::Json& location : locations(key)) {
            if (find(location)) {
                push(location.get<std::string>(), instant);
            }
        }
    }
}

void Router::persist() const {
    engine.getPlugin<plugins::StoragePlugin>().getNavigationState().set(save());
}

void Router::close() {
    if (closed) {
        return;
    }

    if (options.restores) {
        persist();
    }

    closed = true;
    linkConnection = {};
    backgroundConnection = {};
    unmatched.clear();
    navigation.forget(*this);
}

} // namespace haylen::ui
