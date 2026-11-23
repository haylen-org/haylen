#include "ui/NavigationBinding.hpp"

#include <lua.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/ClassBinding.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Message.hpp"
#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/NavigationLink.hpp"
#include "haylen/ui/NavigationSplit.hpp"
#include "haylen/ui/NavigationStack.hpp"
#include "haylen/ui/NavigationTabs.hpp"
#include "haylen/ui/Presentation.hpp"
#include "haylen/ui/Screen.hpp"
#include "runtime/Owners.hpp"
#include "ui/ScriptedScreen.hpp"
#include "ui/UiBinding.hpp"

namespace haylen::lua {

template <> struct Type<haylen::ui::NavigationStack> {
    static constexpr const char* name = "haylen.NavigationStack";
    using Storage = std::shared_ptr<haylen::ui::NavigationStack>;
};

template <> struct Type<haylen::ui::NavigationTabs> {
    static constexpr const char* name = "haylen.NavigationTabs";
    using Storage = std::shared_ptr<haylen::ui::NavigationTabs>;
};

template <> struct Type<haylen::ui::NavigationSplit> {
    static constexpr const char* name = "haylen.NavigationSplit";
    using Storage = std::shared_ptr<haylen::ui::NavigationSplit>;
};

template <> struct Type<haylen::ui::Presentation> {
    static constexpr const char* name = "haylen.Presentation";
    using Storage = std::shared_ptr<haylen::ui::Presentation>;
};

template <> struct Type<haylen::ui::Message> {
    static constexpr const char* name = "haylen.Message";
    using Storage = std::shared_ptr<haylen::ui::Message>;
};

template <> struct Type<haylen::ui::Router> {
    static constexpr const char* name = "haylen.Router";
    using Storage = std::shared_ptr<haylen::ui::Router>;
};

} // namespace haylen::lua

namespace haylen::core {

template <> struct EnumNames<haylen::ui::NavigationTransition::Effect> {
    static std::optional<haylen::ui::NavigationTransition::Effect> fromName(std::string_view name) {
        return haylen::ui::NavigationTransition::effectFromName(name);
    }
    static std::string_view name(haylen::ui::NavigationTransition::Effect value) {
        return haylen::ui::NavigationTransition::effectName(value);
    }
};

template <> struct EnumNames<haylen::ui::Presentation::Style> {
    static std::optional<haylen::ui::Presentation::Style> fromName(std::string_view name) {
        return haylen::ui::Presentation::styleFromName(name);
    }
    static std::string_view name(haylen::ui::Presentation::Style value) {
        return haylen::ui::Presentation::styleName(value);
    }
};

template <> struct EnumNames<haylen::ui::Router::Type> {
    static std::optional<haylen::ui::Router::Type> fromName(std::string_view name) {
        const auto found = std::ranges::find(haylen::ui::Router::kTypeNames, name, &std::pair<std::string_view, haylen::ui::Router::Type>::first);
        return found != haylen::ui::Router::kTypeNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(haylen::ui::Router::Type value) {
        return std::ranges::find(haylen::ui::Router::kTypeNames, value, &std::pair<std::string_view, haylen::ui::Router::Type>::second)->first;
    }
};

template <> struct EnumNames<haylen::ui::Presentation::Size> {
    static std::optional<haylen::ui::Presentation::Size> fromName(std::string_view name) {
        const auto found = std::ranges::find(haylen::ui::Presentation::kSizeNames, name, &std::pair<std::string_view, haylen::ui::Presentation::Size>::first);
        return found != haylen::ui::Presentation::kSizeNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(haylen::ui::Presentation::Size value) {
        return std::ranges::find(haylen::ui::Presentation::kSizeNames, value, &std::pair<std::string_view, haylen::ui::Presentation::Size>::second)->first;
    }
};

template <> struct EnumNames<haylen::ui::Navigation::Alert::Style> {
    static std::optional<haylen::ui::Navigation::Alert::Style> fromName(std::string_view name) {
        const auto found = std::ranges::find(haylen::ui::Navigation::Alert::kStyleNames, name, &std::pair<std::string_view, haylen::ui::Navigation::Alert::Style>::first);
        return found != haylen::ui::Navigation::Alert::kStyleNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(haylen::ui::Navigation::Alert::Style value) {
        return std::ranges::find(haylen::ui::Navigation::Alert::kStyleNames, value, &std::pair<std::string_view, haylen::ui::Navigation::Alert::Style>::second)->first;
    }
};

template <> struct EnumNames<haylen::ui::Navigation::Alert::Role> {
    static std::optional<haylen::ui::Navigation::Alert::Role> fromName(std::string_view name) {
        const auto found = std::ranges::find(haylen::ui::Navigation::Alert::kRoleNames, name, &std::pair<std::string_view, haylen::ui::Navigation::Alert::Role>::first);
        return found != haylen::ui::Navigation::Alert::kRoleNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(haylen::ui::Navigation::Alert::Role value) {
        return std::ranges::find(haylen::ui::Navigation::Alert::kRoleNames, value, &std::pair<std::string_view, haylen::ui::Navigation::Alert::Role>::second)->first;
    }
};

template <> struct EnumNames<haylen::ui::Screen::State> {
    static std::optional<haylen::ui::Screen::State> fromName(std::string_view name) {
        return haylen::ui::Screen::stateFromName(name);
    }
    static std::string_view name(haylen::ui::Screen::State value) {
        return haylen::ui::Screen::stateName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::ui {

haylen::ui::Navigation& NavigationBinding::getNavigation(lua_State* L) {
    return UiBinding::getPlugin(L).getNavigation();
}

template <typename T> void NavigationBinding::pushObject(lua_State* L, const std::shared_ptr<T>& object) {
    if (!object) {
        lua_pushnil(L);
        return;
    }

    lua::Owners::pushWeakTable(L, kObjects, "v");
    if (lua_rawgetp(L, -1, object.get()) == LUA_TUSERDATA) {
        lua_remove(L, -2);
        return;
    }

    lua_pop(L, 1);
    lua::Userdata::emplace<T>(L, object);
    lua_pushvalue(L, -1);
    lua_rawsetp(L, -3, object.get());
    lua_remove(L, -2);
}

void NavigationBinding::pushStack(lua_State* L, const std::shared_ptr<haylen::ui::NavigationStack>& stack) {
    pushObject(L, stack);
}

void NavigationBinding::pushPresentation(lua_State* L, const std::shared_ptr<haylen::ui::Presentation>& presentation) {
    pushObject(L, presentation);
}

haylen::ui::Region NavigationBinding::readRegion(lua_State* L, int index, std::string_view option) {
    const int table = lua_absindex(L, index);
    haylen::ui::Region region;
    lua_getfield(L, table, "gui");
    region.gui = UiBinding::toGui(L, -1);
    if (!region.gui) {
        luaL_error(L, "%s names the GUI that holds its area with \"gui\".", std::string(option).c_str());
    }

    lua_pop(L, 1);
    if (lua_getfield(L, table, "id") != LUA_TSTRING) {
        luaL_error(L, "%s names the node of its area with \"id\".", std::string(option).c_str());
    }

    region.node = lua::Stack::read<std::string>(L, -1);
    lua_pop(L, 1);
    return region;
}

// A transition is a table with an `effect`, a `duration` and an `ease`, each of which keeps the fallback when it is missing.
haylen::ui::NavigationTransition NavigationBinding::readTransition(lua_State* L, int index, const haylen::ui::NavigationTransition& fallback) {
    haylen::ui::NavigationTransition transition = fallback;
    if (lua_isnoneornil(L, index)) {
        return transition;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kTransitionFields});
    lua::Table::readField(L, index, "effect", transition.effect);
    lua::Table::readField(L, index, "duration", transition.duration);
    lua::Table::readField(L, index, "ease", transition.ease);
    if (!(transition.duration >= 0.0F) || !std::isfinite(transition.duration)) {
        luaL_error(L, "The \"duration\" of a navigation transition is a finite number of seconds, 0 or more.");
    }

    return transition;
}

std::shared_ptr<haylen::ui::Screen> NavigationBinding::readScreen(lua_State* L, int index) {
    if (!lua_istable(L, index)) {
        luaL_error(L, "A screen is a table with a \"build\" method, not %s.", luaL_typename(L, index));
    }

    return ScriptedScreen::get(L, index);
}

std::function<std::shared_ptr<haylen::ui::Screen>()> NavigationBinding::readFactory(lua_State* L, int index, std::string_view option) {
    if (!lua_isfunction(L, index)) {
        luaL_error(L, "The \"%s\" option is a function that returns a screen.", std::string(option).c_str());
    }

    auto function = std::make_shared<lua::Reference>(L, index);
    // clang-format off
    return [function]() {
        std::shared_ptr<haylen::ui::Screen> made;
        lua::Runtime::protectedRun(function->getState(), [&](lua_State* state) {
            function->push(state);
            lua_call(state, 0, 1);
            made = readScreen(state, -1);
            lua_pop(state, 1);
        });
        return made;
    };
    // clang-format on
}

int NavigationBinding::pushOwner(lua_State* L, int options) {
    if (lua_getfield(L, options, "owner") != LUA_TNIL) {
        lua::Owners::checkOwner(L, -1);
        return lua_gettop(L);
    }

    lua_pop(L, 1);
    return lua::Owners::pushDefault(L);
}

// Creates a stack with `ui.navigationStack{gui = frame, id = 'middle', root = screen, owner = scene, onChange = function(event) end}`.
int NavigationBinding::navigationStack(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kStackFields});
    haylen::ui::Region region = readRegion(L, 1, "A navigation stack");
    std::shared_ptr<haylen::ui::Screen> root;
    if (lua_getfield(L, 1, "root") != LUA_TNIL) {
        root = readScreen(L, -1);
    }

    lua_pop(L, 1);
    const bool notifies = lua_getfield(L, 1, "onChange") != LUA_TNIL;
    if (notifies) {
        lua::Table::requireType(L, "onChange", LUA_TFUNCTION);
    }

    const int handler = lua_gettop(L);
    const int owner = pushOwner(L, 1);
    const std::shared_ptr<haylen::ui::NavigationStack> stack = getNavigation(L).createStack(std::move(region));
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<haylen::ui::NavigationLink>(std::weak_ptr<haylen::ui::NavigationStack>(stack)));
    }

    if (notifies) {
        auto function = std::make_shared<lua::Reference>(L, handler);
        // clang-format off
        haylen::core::Connection connection = stack->changed.connect([function](haylen::ui::NavigationStack& changed) {
            lua_State* main = function->getState();
            lua::Runtime::runReporting(main, [&] {
                function->push(main);
                lua_createtable(main, 0, 3);
                pushStack(main, changed.shared_from_this());
                lua_setfield(main, -2, "stack");
                ScriptedScreen::push(main, changed.getTop());
                lua_setfield(main, -2, "top");
                lua_pushinteger(main, static_cast<lua_Integer>(changed.size()));
                lua_setfield(main, -2, "size");
                lua::Runtime::protectedCall(main, 1, 0);
            });
        });
        // clang-format on
        if (owner != 0) {
            lua::Owners::add(L, owner, connection);
        }
    }

    // A root that fails leaves no open stack behind, which would keep its handler and take part in the system back.
    if (root) {
        try {
            stack->push(std::move(root), {.effect = haylen::ui::NavigationTransition::Effect::None});
        } catch (...) {
            stack->close();
            throw;
        }
    }

    pushStack(L, stack);
    return 1;
}

// Creates tabs with `ui.navigationTabs{gui = frame, id = 'middle', tabs = {{id = 'home', root = function() return Home() end}}, selected = 'home', owner = scene, onChange = function(event) end}`.
int NavigationBinding::navigationTabs(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kTabsFields});
    haylen::ui::Region region = readRegion(L, 1, "Navigation tabs");
    std::vector<haylen::ui::NavigationTabs::Tab> list;
    if (lua_getfield(L, 1, "tabs") != LUA_TTABLE) {
        return luaL_error(L, "Navigation tabs list their tabs in \"tabs\", each a table with an \"id\" and a \"root\" function.");
    }

    const auto count = static_cast<int>(lua_rawlen(L, -1));
    for (int position = 1; position <= count; ++position) {
        lua_rawgeti(L, -1, position);
        lua::Table::requireItemType(L, "tabs", position, LUA_TTABLE);
        lua::Table::checkFields(L, -1, {kTabFields});
        haylen::ui::NavigationTabs::Tab tab;
        lua::Table::readField(L, -1, "id", tab.id);
        lua_getfield(L, -1, "root");
        tab.root = readFactory(L, -1, "root");
        lua_pop(L, 2);
        list.push_back(std::move(tab));
    }

    lua_pop(L, 1);
    std::string selected;
    lua::Table::readField(L, 1, "selected", selected);
    const bool notifies = lua_getfield(L, 1, "onChange") != LUA_TNIL;
    if (notifies) {
        lua::Table::requireType(L, "onChange", LUA_TFUNCTION);
    }

    const int handler = lua_gettop(L);
    const int owner = pushOwner(L, 1);
    const std::shared_ptr<haylen::ui::NavigationTabs> tabs = getNavigation(L).createTabs(std::move(region), std::move(list), selected);
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<haylen::ui::NavigationLink>(std::weak_ptr<haylen::ui::NavigationTabs>(tabs)));
    }

    if (notifies) {
        auto function = std::make_shared<lua::Reference>(L, handler);
        // clang-format off
        haylen::core::Connection connection = tabs->selectionChanged.connect([function](const std::string& current, const std::string& previous) {
            lua_State* main = function->getState();
            lua::Runtime::runReporting(main, [&] {
                function->push(main);
                lua_createtable(main, 0, 2);
                lua::Stack::push(main, current);
                lua_setfield(main, -2, "selected");
                lua::Stack::push(main, previous);
                lua_setfield(main, -2, "previous");
                lua::Runtime::protectedCall(main, 1, 0);
            });
        });
        // clang-format on
        if (owner != 0) {
            lua::Owners::add(L, owner, connection);
        }
    }

    pushObject(L, tabs);
    return 1;
}

// Creates a split with `ui.navigationSplit{primary = {gui = frame, id = 'list'}, detail = {gui = frame, id = 'detail'}, root = screen, placeholder = function() return Empty() end, collapsed = false, owner = scene}`.
int NavigationBinding::navigationSplit(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kSplitFields});
    if (lua_getfield(L, 1, "primary") != LUA_TTABLE) {
        return luaL_error(L, "A navigation split names the area of its list with \"primary\", such as {gui = frame, id = \"list\"}.");
    }

    lua::Table::checkFields(L, -1, {kRegionFields});
    haylen::ui::Region primary = readRegion(L, -1, "The \"primary\" area of a navigation split");
    lua_pop(L, 1);
    if (lua_getfield(L, 1, "detail") != LUA_TTABLE) {
        return luaL_error(L, "A navigation split names the area of its detail with \"detail\", such as {gui = frame, id = \"detail\"}.");
    }

    lua::Table::checkFields(L, -1, {kRegionFields});
    haylen::ui::Region detail = readRegion(L, -1, "The \"detail\" area of a navigation split");
    lua_pop(L, 1);
    lua_getfield(L, 1, "root");
    std::shared_ptr<haylen::ui::Screen> root = readScreen(L, -1);
    lua_pop(L, 1);
    haylen::ui::NavigationSplit::Factory placeholder;
    if (lua_getfield(L, 1, "placeholder") != LUA_TNIL) {
        placeholder = readFactory(L, -1, "placeholder");
    }

    lua_pop(L, 1);
    bool collapsed = false;
    lua::Table::readField(L, 1, "collapsed", collapsed);
    const int owner = pushOwner(L, 1);
    const std::shared_ptr<haylen::ui::NavigationSplit> split = getNavigation(L).createSplit(std::move(primary), std::move(detail), std::move(root), std::move(placeholder), collapsed);
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<haylen::ui::NavigationLink>(std::weak_ptr<haylen::ui::NavigationSplit>(split)));
    }

    pushObject(L, split);
    return 1;
}

int NavigationBinding::pushHandler(lua_State* L, int options, const char* name) {
    if (lua_getfield(L, options, name) == LUA_TNIL) {
        lua_pop(L, 1);
        return 0;
    }

    lua::Table::requireType(L, name, LUA_TFUNCTION);
    return lua_gettop(L);
}

// A detent is "medium", "large", "fit" or a height in UI units, listed from the lowest to the highest.
std::vector<haylen::ui::Presentation::Detent> NavigationBinding::readDetents(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    std::vector<haylen::ui::Presentation::Detent> detents;
    const auto count = static_cast<int>(lua_rawlen(L, index));
    for (int position = 1; position <= count; ++position) {
        lua_rawgeti(L, index, position);
        if (lua_type(L, -1) == LUA_TNUMBER) {
            detents.push_back({.kind = haylen::ui::Presentation::Detent::Kind::Height, .height = static_cast<float>(lua_tonumber(L, -1))});
            lua_pop(L, 1);
            continue;
        }

        const std::string_view name = lua_type(L, -1) == LUA_TSTRING ? lua::Stack::read<std::string_view>(L, -1) : std::string_view{};
        const auto found = std::ranges::find(haylen::ui::Presentation::kDetentNames, name, &std::pair<std::string_view, haylen::ui::Presentation::Detent::Kind>::first);
        if (found == haylen::ui::Presentation::kDetentNames.end()) {
            luaL_error(L, "A detent is \"medium\", \"large\", \"fit\" or a height in UI units, not %s.", luaL_tolstring(L, -1, nullptr));
        }

        detents.push_back({.kind = found->second});
        lua_pop(L, 1);
    }

    return detents;
}

void NavigationBinding::readPresentOptions(lua_State* L, int index, haylen::ui::Presentation::Options& options) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kPresentFields});
    lua::Table::readField(L, index, "style", options.style);
    lua::Table::readField(L, index, "dismissible", options.dismissible);
    lua::Table::readField(L, index, "modal", options.modal);
    lua::Table::readField(L, index, "maxWidth", options.maxWidth);
    lua::Table::readField(L, index, "maxHeight", options.maxHeight);
    lua::Table::readField(L, index, "size", options.size);
    if (lua_getfield(L, index, "color") != LUA_TNIL) {
        options.color = lua::Stack::read<haylen::math::Color>(L, -1);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, index, "transition") != LUA_TNIL) {
        options.transition = readTransition(L, -1, {});
    }

    lua_pop(L, 1);
    if (lua_getfield(L, index, "detents") != LUA_TNIL) {
        options.detents = readDetents(L, -1);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, index, "detent") != LUA_TNIL) {
        const lua_Integer position = luaL_checkinteger(L, -1);
        if (position < 1 || position > static_cast<lua_Integer>(options.detents.size())) {
            luaL_error(L, "The \"detent\" a sheet opens at is the place of one of its detents, from 1 to %d.", static_cast<int>(options.detents.size()));
        }

        options.detent = static_cast<std::size_t>(position - 1);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, index, "grabber") != LUA_TNIL) {
        options.grabber = lua::Stack::read<bool>(L, -1);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, index, "width") != LUA_TNIL) {
        options.width = lua::Stack::read<float>(L, -1);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, index, "anchor") != LUA_TNIL) {
        lua::Table::requireType(L, "anchor", LUA_TTABLE);
        lua::Table::checkFields(L, -1, {kRegionFields});
        options.anchor = readRegion(L, -1, "The anchor of a popover");
    }

    lua_pop(L, 1);
}

bool NavigationBinding::hasLeft(const haylen::ui::Navigation& navigation, const haylen::ui::Presentation& presentation) {
    const std::vector<std::shared_ptr<haylen::ui::Presentation>>& shown = navigation.getPresentations();
    return presentation.isDismissed() && std::ranges::none_of(shown, [&presentation](const std::shared_ptr<haylen::ui::Presentation>& entry) { return entry.get() == &presentation; });
}

bool NavigationBinding::hasLeft(const haylen::ui::Navigation& navigation, const haylen::ui::Message& message) {
    const std::vector<std::shared_ptr<haylen::ui::Message>>& shown = navigation.getMessages();
    return message.isDismissed() && std::ranges::none_of(shown, [&message](const std::shared_ptr<haylen::ui::Message>& entry) { return entry.get() == &message; });
}

haylen::core::Json NavigationBinding::getOutcome(const haylen::ui::Presentation& presentation) {
    return presentation.getResult();
}

haylen::core::Json NavigationBinding::getOutcome(const haylen::ui::Message& message) {
    return std::string(haylen::ui::Message::reasonName(*message.getReason()));
}

// The promise of the result lives in the userdata, so it lasts while Lua holds the object, and a userdata pushed again for an object that left makes a promise that already holds its result.
template <typename T> void NavigationBinding::pushResult(lua_State* L, int index) {
    const int self = lua_absindex(L, index);
    lua::Userdata::pushField(L, self, "result");
    if (!lua_isnil(L, -1)) {
        return;
    }

    lua_pop(L, 1);
    const std::shared_ptr<T>& object = lua::Userdata::checkShared<T>(L, self);
    lua::Promise promise(lua::Runtime::getEngine(L));
    if (hasLeft(getNavigation(L), *object)) {
        promise.resolve(getOutcome(*object));
    } else {
        // clang-format off
        static_cast<void>(object->dismissed.connect([promise](T& left) {
            promise.resolve(getOutcome(left));
        }));
        // clang-format on
    }

    promise.push(L);
    lua::Userdata::setField(L, self, "result", lua_gettop(L));
}

void NavigationBinding::finishPresentation(lua_State* L, const std::shared_ptr<haylen::ui::Presentation>& presentation, int owner, int dismissHandler, int detentHandler) {
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<haylen::ui::NavigationLink>(std::weak_ptr<haylen::ui::Presentation>(presentation)));
    }

    if (dismissHandler != 0) {
        auto function = std::make_shared<lua::Reference>(L, dismissHandler);
        // clang-format off
        haylen::core::Connection connection = presentation->dismissed.connect([function](haylen::ui::Presentation& left) {
            lua_State* main = function->getState();
            lua::Runtime::runReporting(main, [&] {
                function->push(main);
                lua_createtable(main, 0, 2);
                pushPresentation(main, left.shared_from_this());
                lua_setfield(main, -2, "presentation");
                lua::JsonConverter::push(main, left.getResult());
                lua_setfield(main, -2, "result");
                lua::Runtime::protectedCall(main, 1, 0);
            });
        });
        // clang-format on
        if (owner != 0) {
            lua::Owners::add(L, owner, connection);
        }
    }

    if (detentHandler != 0) {
        auto function = std::make_shared<lua::Reference>(L, detentHandler);
        // clang-format off
        haylen::core::Connection connection = presentation->detentChanged.connect([function](haylen::ui::Presentation& moved) {
            lua_State* main = function->getState();
            lua::Runtime::runReporting(main, [&] {
                function->push(main);
                lua_createtable(main, 0, 2);
                pushPresentation(main, moved.shared_from_this());
                lua_setfield(main, -2, "presentation");
                lua_pushinteger(main, static_cast<lua_Integer>(moved.getDetent() + 1));
                lua_setfield(main, -2, "detent");
                lua::Runtime::protectedCall(main, 1, 0);
            });
        });
        // clang-format on
        if (owner != 0) {
            lua::Owners::add(L, owner, connection);
        }
    }

    pushPresentation(L, presentation);
    pushResult<haylen::ui::Presentation>(L, -1);
    lua_pop(L, 1);
}

// Presents a screen with `ui.present(screen, {style = 'sheet', detents = {'medium', 'large'}, transition = {...}, dismissible = true, owner = scene, onDismiss = function(event) end})`.
int NavigationBinding::present(lua_State* L) {
    std::shared_ptr<haylen::ui::Screen> screen = readScreen(L, 1);
    haylen::ui::Presentation::Options options;
    int dismissHandler = 0;
    int detentHandler = 0;
    int owner = 0;
    if (!lua_isnoneornil(L, 2)) {
        readPresentOptions(L, 2, options);
        dismissHandler = pushHandler(L, 2, "onDismiss");
        detentHandler = pushHandler(L, 2, "onDetentChange");
        owner = pushOwner(L, 2);
    } else {
        owner = lua::Owners::pushDefault(L);
    }

    const std::shared_ptr<haylen::ui::Presentation> presentation = getNavigation(L).present(std::move(screen), std::move(options));
    finishPresentation(L, presentation, owner, dismissHandler, detentHandler);
    return 1;
}

// Presents an alert with `ui.presentAlert{style = 'alert', title = 'Delete?', message = '...', actions = {{id = 'cancel', text = 'Cancel', role = 'cancel'}, {id = 'delete', text = 'Delete', role = 'destructive'}}, anchor = {gui = gui, id = 'share'}, owner = scene, onDismiss = function(event) end}`.
int NavigationBinding::presentAlert(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kAlertFields});
    haylen::ui::Navigation::Alert alert;
    lua::Table::readField(L, 1, "style", alert.style);
    if (lua_getfield(L, 1, "title") != LUA_TNIL) {
        alert.title = lua::JsonConverter::read(L, -1);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, 1, "message") != LUA_TNIL) {
        alert.message = lua::JsonConverter::read(L, -1);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, 1, "actions") != LUA_TTABLE) {
        return luaL_error(L, "An alert lists its actions in \"actions\", each a table with an \"id\", a \"text\" and a \"role\".");
    }

    const auto count = static_cast<int>(lua_rawlen(L, -1));
    for (int position = 1; position <= count; ++position) {
        lua_rawgeti(L, -1, position);
        lua::Table::requireItemType(L, "actions", position, LUA_TTABLE);
        lua::Table::checkFields(L, -1, {kActionFields});
        haylen::ui::Navigation::Alert::Action action;
        lua::Table::readField(L, -1, "id", action.id);
        lua::Table::readField(L, -1, "role", action.role);
        if (lua_getfield(L, -1, "text") != LUA_TNIL) {
            action.text = lua::JsonConverter::read(L, -1);
        }

        lua_pop(L, 2);
        alert.actions.push_back(std::move(action));
    }

    lua_pop(L, 1);
    if (lua_getfield(L, 1, "anchor") != LUA_TNIL) {
        lua::Table::requireType(L, "anchor", LUA_TTABLE);
        lua::Table::checkFields(L, -1, {kRegionFields});
        alert.anchor = readRegion(L, -1, "The anchor of an action sheet");
    }

    lua_pop(L, 1);
    const int dismissHandler = pushHandler(L, 1, "onDismiss");
    const int owner = pushOwner(L, 1);
    const std::shared_ptr<haylen::ui::Presentation> presentation = getNavigation(L).presentAlert(std::move(alert));
    finishPresentation(L, presentation, owner, dismissHandler, 0);
    return 1;
}

haylen::core::Json NavigationBinding::readMessageProperties(lua_State* L, int index, std::span<const std::string_view> keys) {
    const int table = lua_absindex(L, index);
    haylen::core::Json properties = haylen::core::Json::object();
    for (const std::string_view key : keys) {
        lua_pushlstring(L, key.data(), key.size());
        if (lua_gettable(L, table) != LUA_TNIL) {
            properties[std::string(key)] = lua::JsonConverter::read(L, -1);
        }

        lua_pop(L, 1);
    }

    return properties;
}

void NavigationBinding::connectMessage(lua_State* L, haylen::core::Signal<haylen::ui::Message&>& signal, int handler, int owner) {
    if (handler == 0) {
        return;
    }

    auto function = std::make_shared<lua::Reference>(L, handler);
    // clang-format off
    haylen::core::Connection connection = signal.connect([function](haylen::ui::Message& message) {
        lua_State* main = function->getState();
        lua::Runtime::runReporting(main, [&] {
            function->push(main);
            lua_createtable(main, 0, 2);
            pushObject(main, message.shared_from_this());
            lua_setfield(main, -2, "message");
            if (message.getReason()) {
                lua::Stack::push(main, haylen::ui::Message::reasonName(*message.getReason()));
                lua_setfield(main, -2, "reason");
            }

            lua::Runtime::protectedCall(main, 1, 0);
        });
    });
    // clang-format on
    if (owner != 0) {
        lua::Owners::add(L, owner, connection);
    }
}

// Shows a message with `ui.showMessage{style = 'snackbar', text = 'Photo deleted.', action = 'Undo', priority = 'high', owner = scene, onAction = function(event) end, onDismiss = function(event) end}`.
int NavigationBinding::showMessage(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {haylen::ui::Message::kContentKeys, haylen::ui::Message::kPlacementKeys, kMessageHandlerFields});
    haylen::core::Json properties = readMessageProperties(L, 1, haylen::ui::Message::kPlacementKeys);
    properties.update(readMessageProperties(L, 1, haylen::ui::Message::kContentKeys));
    const int showHandler = pushHandler(L, 1, "onShow");
    const int actionHandler = pushHandler(L, 1, "onAction");
    const int tapHandler = pushHandler(L, 1, "onTap");
    const int dismissHandler = pushHandler(L, 1, "onDismiss");
    const int owner = pushOwner(L, 1);

    const std::shared_ptr<haylen::ui::Message> message = getNavigation(L).showMessage(properties);
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<haylen::ui::NavigationLink>(std::weak_ptr<haylen::ui::Message>(message)));
    }

    connectMessage(L, message->appeared, showHandler, owner);
    connectMessage(L, message->actionPressed, actionHandler, owner);
    connectMessage(L, message->tapped, tapHandler, owner);
    connectMessage(L, message->dismissed, dismissHandler, owner);
    pushObject(L, message);
    pushResult<haylen::ui::Message>(L, -1);
    lua_pop(L, 1);
    return 1;
}

// A route reads `{name = 'artist', path = 'artist/{id}', query = {tab = 'string'}, tab = 'search', parent = 'search', detail = false, present = {style = 'sheet'}, screen = function(parameters) return Artist(parameters.id) end}`.
haylen::ui::Router::Route NavigationBinding::readRoute(lua_State* L, int index) {
    const int table = lua_absindex(L, index);
    luaL_checktype(L, table, LUA_TTABLE);
    lua::Table::checkFields(L, table, {kRouteFields});
    haylen::ui::Router::Route route;
    lua::Table::readField(L, table, "name", route.name);
    lua::Table::readField(L, table, "path", route.path);
    lua::Table::readField(L, table, "tab", route.tab);
    lua::Table::readField(L, table, "parent", route.parent);
    lua::Table::readField(L, table, "detail", route.detail);
    if (lua_getfield(L, table, "query") != LUA_TNIL) {
        lua::Table::requireType(L, "query", LUA_TTABLE);
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            const std::string name = lua::Stack::read<std::string>(L, -2);
            haylen::ui::Router::Type type = haylen::ui::Router::Type::String;
            lua::Table::readValue(L, "query", type);
            route.query.emplace_back(name, type);
            lua_pop(L, 1);
        }

        std::ranges::sort(route.query, {}, &std::pair<std::string, haylen::ui::Router::Type>::first);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, table, "present") != LUA_TNIL) {
        haylen::ui::Presentation::Options presentation;
        readPresentOptions(L, -1, presentation);
        route.presentation = std::move(presentation);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, table, "screen") != LUA_TNIL) {
        if (!lua_isfunction(L, -1)) {
            luaL_error(L, "The \"screen\" of a route is a function that returns a screen for the parameters of the route.");
        }

        auto function = std::make_shared<lua::Reference>(L, -1);
        // clang-format off
        route.screen = [function](const haylen::core::Json& parameters) {
            std::shared_ptr<haylen::ui::Screen> made;
            lua::Runtime::protectedRun(function->getState(), [&](lua_State* state) {
                function->push(state);
                lua::JsonConverter::push(state, parameters);
                lua_call(state, 1, 1);
                made = readScreen(state, -1);
                lua_pop(state, 1);
            });
            return made;
        };
        // clang-format on
    }

    lua_pop(L, 1);
    return route;
}

// Creates a router with `ui.router{tabs = tabs, schemes = {'music'}, hosts = {'music.example.com'}, links = true, restore = true, routes = {...}, owner = scene, onUnmatched = function(event) end}`.
int NavigationBinding::router(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kRouterFields});
    haylen::ui::Router::Options options;
    if (lua_getfield(L, 1, "tabs") != LUA_TNIL) {
        options.tabs = lua::Userdata::checkShared<haylen::ui::NavigationTabs>(L, -1);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, 1, "stack") != LUA_TNIL) {
        options.stack = lua::Userdata::checkShared<haylen::ui::NavigationStack>(L, -1);
    }

    lua_pop(L, 1);
    lua::Table::readField(L, 1, "schemes", options.schemes);
    lua::Table::readField(L, 1, "hosts", options.hosts);
    lua::Table::readField(L, 1, "links", options.links);
    lua::Table::readField(L, 1, "restore", options.restores);
    if (lua_getfield(L, 1, "routes") != LUA_TTABLE) {
        return luaL_error(L, "A router lists its routes in \"routes\", each a table with a \"name\" and a \"path\".");
    }

    const auto count = static_cast<int>(lua_rawlen(L, -1));
    for (int position = 1; position <= count; ++position) {
        lua_rawgeti(L, -1, position);
        options.routes.push_back(readRoute(L, -1));
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    const int handler = pushHandler(L, 1, "onUnmatched");
    const int owner = pushOwner(L, 1);
    const std::shared_ptr<haylen::ui::Router> created = getNavigation(L).createRouter(std::move(options));
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<haylen::ui::NavigationLink>(std::weak_ptr<haylen::ui::Router>(created)));
    }

    if (handler != 0) {
        auto function = std::make_shared<lua::Reference>(L, handler);
        // clang-format off
        haylen::core::Connection connection = created->unmatched.connect([function](const std::string& url) {
            lua_State* main = function->getState();
            lua::Runtime::runReporting(main, [&] {
                function->push(main);
                lua_createtable(main, 0, 1);
                lua::Stack::push(main, url);
                lua_setfield(main, -2, "url");
                lua::Runtime::protectedCall(main, 1, 0);
            });
        });
        // clang-format on
        if (owner != 0) {
            lua::Owners::add(L, owner, connection);
        }
    }

    pushObject(L, created);
    return 1;
}

int NavigationBinding::routerOpen(lua_State* L) {
    haylen::ui::Router& router = lua::Userdata::check<haylen::ui::Router>(L, 1);
    const std::string link = lua::Stack::read<std::string>(L, 2);
    router.open(link, readTransition(L, 3, {}));
    return 0;
}

int NavigationBinding::routerPush(lua_State* L) {
    haylen::ui::Router& router = lua::Userdata::check<haylen::ui::Router>(L, 1);
    const std::string location = lua::Stack::read<std::string>(L, 2);
    router.push(location, readTransition(L, 3, {}));
    return 0;
}

// Returns the name of the route a link matches and its parameters, or nothing.
int NavigationBinding::routerMatch(lua_State* L) {
    const haylen::ui::Router& router = lua::Userdata::check<haylen::ui::Router>(L, 1);
    const std::optional<haylen::ui::Router::Match> found = router.match(lua::Stack::read<std::string_view>(L, 2));
    if (!found) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, found->route->name);
    lua::JsonConverter::push(L, found->parameters);
    return 2;
}

int NavigationBinding::routerLocation(lua_State* L) {
    const haylen::ui::Router& router = lua::Userdata::check<haylen::ui::Router>(L, 1);
    const std::shared_ptr<haylen::ui::Screen> screen = readScreen(L, 2);
    const std::string location = router.getLocation(*screen);
    if (location.empty()) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, location);
    return 1;
}

int NavigationBinding::routerSave(lua_State* L) {
    lua::JsonConverter::push(L, lua::Userdata::check<haylen::ui::Router>(L, 1).save());
    return 1;
}

int NavigationBinding::routerRestore(lua_State* L) {
    haylen::ui::Router& router = lua::Userdata::check<haylen::ui::Router>(L, 1);
    router.restore(lua::JsonConverter::read(L, 2));
    return 0;
}

int NavigationBinding::routerClose(lua_State* L) {
    lua::Userdata::check<haylen::ui::Router>(L, 1).close();
    return 0;
}

int NavigationBinding::routerClosed(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<haylen::ui::Router>(L, 1).isClosed() ? 1 : 0);
    return 1;
}

// Keeps the messages inside the area of a node with `ui.setMessageArea{gui = frame, id = 'middle'}`, or in the safe area again with `ui.setMessageArea(nil)`.
int NavigationBinding::setMessageArea(lua_State* L) {
    if (lua_isnoneornil(L, 1)) {
        getNavigation(L).setMessageArea(std::nullopt);
        return 0;
    }

    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kRegionFields});
    getNavigation(L).setMessageArea(readRegion(L, 1, "The area of the messages"));
    return 0;
}

int NavigationBinding::goBack(lua_State* L) {
    lua_pushboolean(L, getNavigation(L).goBack() ? 1 : 0);
    return 1;
}

int NavigationBinding::canGoBack(lua_State* L) {
    lua_pushboolean(L, getNavigation(L).canGoBack() ? 1 : 0);
    return 1;
}

int NavigationBinding::screenState(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "A screen is a table with a \"build\" method, not %s.", luaL_typename(L, 1));
    }

    lua::Stack::push(L, haylen::core::EnumNames<haylen::ui::Screen::State>::name(ScriptedScreen::findState(L, 1)));
    return 1;
}

int NavigationBinding::stackPush(lua_State* L) {
    haylen::ui::NavigationStack& stack = lua::Userdata::check<haylen::ui::NavigationStack>(L, 1);
    std::shared_ptr<haylen::ui::Screen> screen = readScreen(L, 2);
    stack.push(std::move(screen), readTransition(L, 3, {}));
    return 0;
}

int NavigationBinding::stackPop(lua_State* L) {
    lua::Userdata::check<haylen::ui::NavigationStack>(L, 1).pop(readTransition(L, 2, {}));
    return 0;
}

int NavigationBinding::stackPopTo(lua_State* L) {
    haylen::ui::NavigationStack& stack = lua::Userdata::check<haylen::ui::NavigationStack>(L, 1);
    const std::shared_ptr<haylen::ui::Screen> screen = readScreen(L, 2);
    stack.popTo(*screen, readTransition(L, 3, {}));
    return 0;
}

int NavigationBinding::stackPopToRoot(lua_State* L) {
    lua::Userdata::check<haylen::ui::NavigationStack>(L, 1).popToRoot(readTransition(L, 2, {}));
    return 0;
}

int NavigationBinding::stackReplace(lua_State* L) {
    haylen::ui::NavigationStack& stack = lua::Userdata::check<haylen::ui::NavigationStack>(L, 1);
    std::shared_ptr<haylen::ui::Screen> screen = readScreen(L, 2);
    stack.replace(std::move(screen), readTransition(L, 3, {}));
    return 0;
}

int NavigationBinding::stackSetScreens(lua_State* L) {
    haylen::ui::NavigationStack& stack = lua::Userdata::check<haylen::ui::NavigationStack>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<std::shared_ptr<haylen::ui::Screen>> next;
    const auto count = static_cast<int>(lua_rawlen(L, 2));
    for (int position = 1; position <= count; ++position) {
        lua_rawgeti(L, 2, position);
        next.push_back(readScreen(L, -1));
        lua_pop(L, 1);
    }

    stack.setScreens(std::move(next), readTransition(L, 3, {}));
    return 0;
}

int NavigationBinding::stackClose(lua_State* L) {
    lua::Userdata::check<haylen::ui::NavigationStack>(L, 1).close();
    return 0;
}

int NavigationBinding::stackScreens(lua_State* L) {
    const std::vector<std::shared_ptr<haylen::ui::Screen>>& screens = lua::Userdata::check<haylen::ui::NavigationStack>(L, 1).getScreens();
    lua_createtable(L, static_cast<int>(screens.size()), 0);
    for (std::size_t position = 0; position < screens.size(); ++position) {
        ScriptedScreen::push(L, screens[position]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(position + 1));
    }

    return 1;
}

int NavigationBinding::stackTop(lua_State* L) {
    ScriptedScreen::push(L, lua::Userdata::check<haylen::ui::NavigationStack>(L, 1).getTop());
    return 1;
}

int NavigationBinding::stackSize(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(lua::Userdata::check<haylen::ui::NavigationStack>(L, 1).size()));
    return 1;
}

int NavigationBinding::stackCanGoBack(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<haylen::ui::NavigationStack>(L, 1).canGoBack() ? 1 : 0);
    return 1;
}

int NavigationBinding::stackTransitioning(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<haylen::ui::NavigationStack>(L, 1).isTransitioning() ? 1 : 0);
    return 1;
}

int NavigationBinding::stackShown(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<haylen::ui::NavigationStack>(L, 1).isShown() ? 1 : 0);
    return 1;
}

int NavigationBinding::stackSetShown(lua_State* L) {
    lua::Userdata::check<haylen::ui::NavigationStack>(L, 1).setShown(lua::Stack::read<bool>(L, 3));
    return 0;
}

int NavigationBinding::stackClosed(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<haylen::ui::NavigationStack>(L, 1).isClosed() ? 1 : 0);
    return 1;
}

int NavigationBinding::tabsSelect(lua_State* L) {
    lua::Userdata::check<haylen::ui::NavigationTabs>(L, 1).select(lua::Stack::read<std::string_view>(L, 2));
    return 0;
}

int NavigationBinding::tabsStack(lua_State* L) {
    pushStack(L, lua::Userdata::check<haylen::ui::NavigationTabs>(L, 1).getStack(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int NavigationBinding::tabsClose(lua_State* L) {
    lua::Userdata::check<haylen::ui::NavigationTabs>(L, 1).close();
    return 0;
}

int NavigationBinding::tabsSelected(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::ui::NavigationTabs>(L, 1).getSelected());
    return 1;
}

int NavigationBinding::tabsCurrent(lua_State* L) {
    haylen::ui::NavigationTabs& tabs = lua::Userdata::check<haylen::ui::NavigationTabs>(L, 1);
    if (tabs.isClosed()) {
        lua_pushnil(L);
        return 1;
    }

    pushStack(L, tabs.getCurrent());
    return 1;
}

int NavigationBinding::tabsClosed(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<haylen::ui::NavigationTabs>(L, 1).isClosed() ? 1 : 0);
    return 1;
}

int NavigationBinding::splitShowDetail(lua_State* L) {
    haylen::ui::NavigationSplit& split = lua::Userdata::check<haylen::ui::NavigationSplit>(L, 1);
    std::shared_ptr<haylen::ui::Screen> screen = readScreen(L, 2);
    const std::optional<haylen::ui::NavigationTransition> transition = lua_isnoneornil(L, 3) ? std::nullopt : std::optional(readTransition(L, 3, {}));
    split.showDetail(std::move(screen), transition);
    return 0;
}

int NavigationBinding::splitClearDetail(lua_State* L) {
    haylen::ui::NavigationSplit& split = lua::Userdata::check<haylen::ui::NavigationSplit>(L, 1);
    const std::optional<haylen::ui::NavigationTransition> transition = lua_isnoneornil(L, 2) ? std::nullopt : std::optional(readTransition(L, 2, {}));
    split.clearDetail(transition);
    return 0;
}

int NavigationBinding::splitClose(lua_State* L) {
    lua::Userdata::check<haylen::ui::NavigationSplit>(L, 1).close();
    return 0;
}

int NavigationBinding::splitCollapsed(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<haylen::ui::NavigationSplit>(L, 1).isCollapsed() ? 1 : 0);
    return 1;
}

int NavigationBinding::splitSetCollapsed(lua_State* L) {
    lua::Userdata::check<haylen::ui::NavigationSplit>(L, 1).setCollapsed(lua::Stack::read<bool>(L, 3));
    return 0;
}

int NavigationBinding::splitHasDetail(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<haylen::ui::NavigationSplit>(L, 1).hasDetail() ? 1 : 0);
    return 1;
}

int NavigationBinding::splitPrimary(lua_State* L) {
    pushStack(L, lua::Userdata::check<haylen::ui::NavigationSplit>(L, 1).getPrimary());
    return 1;
}

int NavigationBinding::splitDetail(lua_State* L) {
    pushStack(L, lua::Userdata::check<haylen::ui::NavigationSplit>(L, 1).getDetail());
    return 1;
}

int NavigationBinding::splitClosed(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<haylen::ui::NavigationSplit>(L, 1).isClosed() ? 1 : 0);
    return 1;
}

// Dismisses with `presentation:dismiss{result = value, transition = {...}}`, where the result is a value that JSON holds.
int NavigationBinding::presentationDismiss(lua_State* L) {
    haylen::ui::Presentation& presentation = lua::Userdata::check<haylen::ui::Presentation>(L, 1);
    haylen::core::Json result;
    std::optional<haylen::ui::NavigationTransition> transition;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kDismissFields});
        if (lua_getfield(L, 2, "result") != LUA_TNIL) {
            result = lua::JsonConverter::read(L, -1);
        }

        lua_pop(L, 1);
        if (lua_getfield(L, 2, "transition") != LUA_TNIL) {
            transition = readTransition(L, -1, {});
        }

        lua_pop(L, 1);
    }

    presentation.dismiss(std::move(result), transition);
    return 0;
}

int NavigationBinding::presentationScreen(lua_State* L) {
    ScriptedScreen::push(L, lua::Userdata::check<haylen::ui::Presentation>(L, 1).getScreen());
    return 1;
}

int NavigationBinding::presentationStyle(lua_State* L) {
    lua::Stack::push(L, haylen::ui::Presentation::styleName(lua::Userdata::check<haylen::ui::Presentation>(L, 1).getStyle()));
    return 1;
}

int NavigationBinding::presentationDismissible(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<haylen::ui::Presentation>(L, 1).isDismissible() ? 1 : 0);
    return 1;
}

int NavigationBinding::presentationDismissed(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<haylen::ui::Presentation>(L, 1).isDismissed() ? 1 : 0);
    return 1;
}

int NavigationBinding::presentationModal(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<haylen::ui::Presentation>(L, 1).isModal() ? 1 : 0);
    return 1;
}

int NavigationBinding::presentationResult(lua_State* L) {
    static_cast<void>(lua::Userdata::check<haylen::ui::Presentation>(L, 1));
    pushResult<haylen::ui::Presentation>(L, 1);
    return 1;
}

int NavigationBinding::presentationDetent(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(lua::Userdata::check<haylen::ui::Presentation>(L, 1).getDetent() + 1));
    return 1;
}

int NavigationBinding::presentationSetDetent(lua_State* L) {
    haylen::ui::Presentation& presentation = lua::Userdata::check<haylen::ui::Presentation>(L, 1);
    const lua_Integer position = luaL_checkinteger(L, 3);
    const auto count = static_cast<lua_Integer>(presentation.getOptions().detents.size());
    if (position < 1 || position > count) {
        return luaL_error(L, "A sheet rests at the place of one of its detents, from 1 to %d.", static_cast<int>(count));
    }

    presentation.setDetent(static_cast<std::size_t>(position - 1));
    return 0;
}

// Changes a message with `message:update{text = 'Uploaded.', tone = 'success', indicator = 'icon', duration = 2}`.
int NavigationBinding::messageUpdate(lua_State* L) {
    haylen::ui::Message& message = lua::Userdata::check<haylen::ui::Message>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {haylen::ui::Message::kContentKeys});
    message.update(readMessageProperties(L, 2, haylen::ui::Message::kContentKeys));
    return 0;
}

int NavigationBinding::messageDismiss(lua_State* L) {
    lua::Userdata::check<haylen::ui::Message>(L, 1).dismiss();
    return 0;
}

int NavigationBinding::messageStyle(lua_State* L) {
    lua::Stack::push(L, haylen::ui::Message::styleName(lua::Userdata::check<haylen::ui::Message>(L, 1).getStyle()));
    return 1;
}

int NavigationBinding::messageShown(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<haylen::ui::Message>(L, 1).isShown() ? 1 : 0);
    return 1;
}

int NavigationBinding::messageDismissed(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<haylen::ui::Message>(L, 1).isDismissed() ? 1 : 0);
    return 1;
}

int NavigationBinding::messageResult(lua_State* L) {
    static_cast<void>(lua::Userdata::check<haylen::ui::Message>(L, 1));
    pushResult<haylen::ui::Message>(L, 1);
    return 1;
}

void NavigationBinding::addTo(lua_State* L, int module) {
    const int target = lua_absindex(L, module);
    // clang-format off
    const luaL_Reg functions[] = {
        {"navigationStack", &lua::Binding::native<&navigationStack>},
        {"navigationTabs", &lua::Binding::native<&navigationTabs>},
        {"navigationSplit", &lua::Binding::native<&navigationSplit>},
        {"present", &lua::Binding::native<&present>},
        {"presentAlert", &lua::Binding::native<&presentAlert>},
        {"showMessage", &lua::Binding::native<&showMessage>},
        {"setMessageArea", &lua::Binding::native<&setMessageArea>},
        {"router", &lua::Binding::native<&router>},
        {"goBack", &lua::Binding::native<&goBack>},
        {"canGoBack", &lua::Binding::native<&canGoBack>},
        {"screenState", &lua::Binding::native<&screenState>},
        {nullptr, nullptr},
    };
    // clang-format on
    for (const luaL_Reg* entry = functions; entry->name != nullptr; ++entry) {
        lua_pushcfunction(L, entry->func);
        lua_setfield(L, target, entry->name);
    }

    // The `Screen` base class names the screens that apps build with `haylen.class`.
    lua::core::ClassBinding::push(L);
    lua_pushliteral(L, "Screen");
    lua_call(L, 1, 1);
    lua_setfield(L, target, "Screen");
}

void NavigationBinding::install(lua_State* L) {
    // clang-format off
    lua::ClassBuilder<haylen::ui::NavigationStack>(L)
        .function("push", &lua::Binding::native<&stackPush>)
        .function("pop", &lua::Binding::native<&stackPop>)
        .function("popTo", &lua::Binding::native<&stackPopTo>)
        .function("popToRoot", &lua::Binding::native<&stackPopToRoot>)
        .function("replace", &lua::Binding::native<&stackReplace>)
        .function("setScreens", &lua::Binding::native<&stackSetScreens>)
        .function("close", &lua::Binding::native<&stackClose>)
        .property("screens", &stackScreens)
        .property("top", &stackTop)
        .property("size", &stackSize)
        .property("canGoBack", &stackCanGoBack)
        .property("transitioning", &stackTransitioning)
        .property("shown", &stackShown, &lua::Binding::native<&stackSetShown>)
        .property("closed", &stackClosed)
        .install();

    lua::ClassBuilder<haylen::ui::NavigationTabs>(L)
        .function("select", &lua::Binding::native<&tabsSelect>)
        .function("stack", &lua::Binding::native<&tabsStack>)
        .function("close", &lua::Binding::native<&tabsClose>)
        .property("selected", &tabsSelected)
        .property("current", &lua::Binding::native<&tabsCurrent>)
        .property("closed", &tabsClosed)
        .install();

    lua::ClassBuilder<haylen::ui::NavigationSplit>(L)
        .function("showDetail", &lua::Binding::native<&splitShowDetail>)
        .function("clearDetail", &lua::Binding::native<&splitClearDetail>)
        .function("close", &lua::Binding::native<&splitClose>)
        .property("collapsed", &splitCollapsed, &lua::Binding::native<&splitSetCollapsed>)
        .property("hasDetail", &splitHasDetail)
        .property("primary", &splitPrimary)
        .property("detail", &splitDetail)
        .property("closed", &splitClosed)
        .install();

    lua::ClassBuilder<haylen::ui::Presentation>(L)
        .function("dismiss", &lua::Binding::native<&presentationDismiss>)
        .property("screen", &presentationScreen)
        .property("style", &presentationStyle)
        .property("dismissible", &presentationDismissible)
        .property("dismissed", &presentationDismissed)
        .property("modal", &presentationModal)
        .property("result", &lua::Binding::native<&presentationResult>)
        .property("detent", &presentationDetent, &lua::Binding::native<&presentationSetDetent>)
        .install();

    lua::ClassBuilder<haylen::ui::Message>(L)
        .function("update", &lua::Binding::native<&messageUpdate>)
        .function("dismiss", &lua::Binding::native<&messageDismiss>)
        .property("style", &messageStyle)
        .property("shown", &messageShown)
        .property("dismissed", &messageDismissed)
        .property("result", &lua::Binding::native<&messageResult>)
        .install();

    lua::ClassBuilder<haylen::ui::Router>(L)
        .function("open", &lua::Binding::native<&routerOpen>)
        .function("push", &lua::Binding::native<&routerPush>)
        .function("match", &lua::Binding::native<&routerMatch>)
        .function("location", &lua::Binding::native<&routerLocation>)
        .function("save", &lua::Binding::native<&routerSave>)
        .function("restore", &lua::Binding::native<&routerRestore>)
        .function("close", &lua::Binding::native<&routerClose>)
        .property("closed", &routerClosed)
        .install();
    // clang-format on
}

} // namespace haylen::lua::ui
