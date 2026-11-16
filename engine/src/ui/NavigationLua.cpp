#include "ui/NavigationLua.hpp"

#include <lua.hpp>

#include <algorithm>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/ClassLua.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/EnumNames.hpp"
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
#include "haylen/ui/NavigationSplit.hpp"
#include "haylen/ui/NavigationStack.hpp"
#include "haylen/ui/NavigationTabs.hpp"
#include "haylen/ui/Presentation.hpp"
#include "haylen/ui/Screen.hpp"
#include "lua/Owners.hpp"
#include "ui/MessageContent.hpp"
#include "ui/NavigationLink.hpp"
#include "ui/ScriptedScreen.hpp"
#include "ui/UiLua.hpp"

namespace haylen::lua {

template <> struct Type<ui::NavigationStack> {
    static constexpr const char* name = "haylen.NavigationStack";
    using Storage = std::shared_ptr<ui::NavigationStack>;
};

template <> struct Type<ui::NavigationTabs> {
    static constexpr const char* name = "haylen.NavigationTabs";
    using Storage = std::shared_ptr<ui::NavigationTabs>;
};

template <> struct Type<ui::NavigationSplit> {
    static constexpr const char* name = "haylen.NavigationSplit";
    using Storage = std::shared_ptr<ui::NavigationSplit>;
};

template <> struct Type<ui::Presentation> {
    static constexpr const char* name = "haylen.Presentation";
    using Storage = std::shared_ptr<ui::Presentation>;
};

template <> struct Type<ui::Message> {
    static constexpr const char* name = "haylen.Message";
    using Storage = std::shared_ptr<ui::Message>;
};

template <> struct EnumNames<ui::NavigationTransition::Effect> {
    static std::optional<ui::NavigationTransition::Effect> fromName(std::string_view name) {
        return ui::NavigationTransition::effectFromName(name);
    }
    static std::string_view name(ui::NavigationTransition::Effect value) {
        return ui::NavigationTransition::effectName(value);
    }
};

template <> struct EnumNames<ui::Presentation::Style> {
    static std::optional<ui::Presentation::Style> fromName(std::string_view name) {
        return ui::Presentation::styleFromName(name);
    }
    static std::string_view name(ui::Presentation::Style value) {
        return ui::Presentation::styleName(value);
    }
};

template <> struct Type<ui::Router> {
    static constexpr const char* name = "haylen.Router";
    using Storage = std::shared_ptr<ui::Router>;
};

template <> struct EnumNames<ui::Router::Type> {
    static std::optional<ui::Router::Type> fromName(std::string_view name) {
        const auto found = std::ranges::find(ui::Router::kTypeNames, name, &std::pair<std::string_view, ui::Router::Type>::first);
        return found != ui::Router::kTypeNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(ui::Router::Type value) {
        return std::ranges::find(ui::Router::kTypeNames, value, &std::pair<std::string_view, ui::Router::Type>::second)->first;
    }
};

template <> struct EnumNames<ui::Presentation::Size> {
    static std::optional<ui::Presentation::Size> fromName(std::string_view name) {
        const auto found = std::ranges::find(ui::Presentation::kSizeNames, name, &std::pair<std::string_view, ui::Presentation::Size>::first);
        return found != ui::Presentation::kSizeNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(ui::Presentation::Size value) {
        return std::ranges::find(ui::Presentation::kSizeNames, value, &std::pair<std::string_view, ui::Presentation::Size>::second)->first;
    }
};

template <> struct EnumNames<ui::Navigation::Alert::Style> {
    static std::optional<ui::Navigation::Alert::Style> fromName(std::string_view name) {
        const auto found = std::ranges::find(ui::Navigation::Alert::kStyleNames, name, &std::pair<std::string_view, ui::Navigation::Alert::Style>::first);
        return found != ui::Navigation::Alert::kStyleNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(ui::Navigation::Alert::Style value) {
        return std::ranges::find(ui::Navigation::Alert::kStyleNames, value, &std::pair<std::string_view, ui::Navigation::Alert::Style>::second)->first;
    }
};

template <> struct EnumNames<ui::Navigation::Alert::Role> {
    static std::optional<ui::Navigation::Alert::Role> fromName(std::string_view name) {
        const auto found = std::ranges::find(ui::Navigation::Alert::kRoleNames, name, &std::pair<std::string_view, ui::Navigation::Alert::Role>::first);
        return found != ui::Navigation::Alert::kRoleNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(ui::Navigation::Alert::Role value) {
        return std::ranges::find(ui::Navigation::Alert::kRoleNames, value, &std::pair<std::string_view, ui::Navigation::Alert::Role>::second)->first;
    }
};

template <> struct EnumNames<ui::Screen::State> {
    static std::optional<ui::Screen::State> fromName(std::string_view name) {
        return ui::Screen::stateFromName(name);
    }
    static std::string_view name(ui::Screen::State value) {
        return ui::Screen::stateName(value);
    }
};

} // namespace haylen::lua

namespace haylen::ui {

Navigation& NavigationLua::getNavigation(lua_State* L) {
    return UiLua::getPlugin(L).getNavigation();
}

template <typename T> void NavigationLua::pushObject(lua_State* L, const std::shared_ptr<T>& object) {
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

void NavigationLua::pushStack(lua_State* L, const std::shared_ptr<NavigationStack>& stack) {
    pushObject(L, stack);
}

void NavigationLua::pushPresentation(lua_State* L, const std::shared_ptr<Presentation>& presentation) {
    pushObject(L, presentation);
}

Region NavigationLua::readRegion(lua_State* L, int index, std::string_view option) {
    const int table = lua_absindex(L, index);
    Region region;
    lua_getfield(L, table, "gui");
    region.gui = UiLua::toGui(L, -1);
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
NavigationTransition NavigationLua::readTransition(lua_State* L, int index, const NavigationTransition& fallback) {
    NavigationTransition transition = fallback;
    if (lua_isnoneornil(L, index)) {
        return transition;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kTransitionFields});
    lua::Table::readField(L, index, "effect", transition.effect);
    lua::Table::readField(L, index, "duration", transition.duration);
    lua::Table::readField(L, index, "ease", transition.ease);
    if (transition.duration < 0.0F) {
        luaL_error(L, "The \"duration\" of a navigation transition cannot be negative.");
    }

    return transition;
}

std::shared_ptr<Screen> NavigationLua::readScreen(lua_State* L, int index) {
    if (!lua_istable(L, index)) {
        luaL_error(L, "A screen is a table with a \"build\" method, not %s.", luaL_typename(L, index));
    }

    return ScriptedScreen::get(L, index);
}

std::function<std::shared_ptr<Screen>()> NavigationLua::readFactory(lua_State* L, int index, std::string_view option) {
    if (!lua_isfunction(L, index)) {
        luaL_error(L, "The \"%s\" option is a function that returns a screen.", std::string(option).c_str());
    }

    auto function = std::make_shared<lua::Reference>(L, index);
    // clang-format off
    return [function]() {
        std::shared_ptr<Screen> made;
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

int NavigationLua::pushOwner(lua_State* L, int options) {
    if (lua_getfield(L, options, "owner") != LUA_TNIL) {
        lua::Owners::checkOwner(L, -1);
        return lua_gettop(L);
    }

    lua_pop(L, 1);
    return lua::Owners::pushDefault(L);
}

// Creates a stack with `ui.navigationStack{gui = frame, id = 'middle', root = screen, owner = scene, onChange = function(event) end}`.
int NavigationLua::navigationStack(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kStackFields});
    Region region = readRegion(L, 1, "A navigation stack");
    std::shared_ptr<Screen> root;
    if (lua_getfield(L, 1, "root") != LUA_TNIL) {
        root = readScreen(L, -1);
    }

    lua_pop(L, 1);
    const bool notifies = lua_getfield(L, 1, "onChange") != LUA_TNIL;
    if (notifies) {
        luaL_checktype(L, -1, LUA_TFUNCTION);
    }

    const int handler = lua_gettop(L);
    const int owner = pushOwner(L, 1);
    const std::shared_ptr<NavigationStack> stack = getNavigation(L).createStack(std::move(region));
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<NavigationLink>(std::weak_ptr<NavigationStack>(stack)));
    }

    if (notifies) {
        auto function = std::make_shared<lua::Reference>(L, handler);
        // clang-format off
        core::Connection connection = stack->changed.connect([function](NavigationStack& changed) {
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

    if (root) {
        stack->push(std::move(root), {.effect = NavigationTransition::Effect::None});
    }

    pushStack(L, stack);
    return 1;
}

// Creates tabs with `ui.navigationTabs{gui = frame, id = 'middle', tabs = {{id = 'home', root = function() return Home() end}}, selected = 'home', owner = scene, onChange = function(event) end}`.
int NavigationLua::navigationTabs(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kTabsFields});
    Region region = readRegion(L, 1, "Navigation tabs");
    std::vector<NavigationTabs::Tab> list;
    if (lua_getfield(L, 1, "tabs") != LUA_TTABLE) {
        return luaL_error(L, "Navigation tabs list their tabs in \"tabs\", each a table with an \"id\" and a \"root\" function.");
    }

    const auto count = static_cast<int>(lua_rawlen(L, -1));
    for (int position = 1; position <= count; ++position) {
        lua_rawgeti(L, -1, position);
        luaL_checktype(L, -1, LUA_TTABLE);
        lua::Table::checkFields(L, -1, {kTabFields});
        NavigationTabs::Tab tab;
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
        luaL_checktype(L, -1, LUA_TFUNCTION);
    }

    const int handler = lua_gettop(L);
    const int owner = pushOwner(L, 1);
    const std::shared_ptr<NavigationTabs> tabs = getNavigation(L).createTabs(std::move(region), std::move(list), selected);
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<NavigationLink>(std::weak_ptr<NavigationTabs>(tabs)));
    }

    if (notifies) {
        auto function = std::make_shared<lua::Reference>(L, handler);
        // clang-format off
        core::Connection connection = tabs->selectionChanged.connect([function](const std::string& current, const std::string& previous) {
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
int NavigationLua::navigationSplit(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kSplitFields});
    if (lua_getfield(L, 1, "primary") != LUA_TTABLE) {
        return luaL_error(L, "A navigation split names the area of its list with \"primary\", such as {gui = frame, id = \"list\"}.");
    }

    lua::Table::checkFields(L, -1, {kRegionFields});
    Region primary = readRegion(L, -1, "The \"primary\" area of a navigation split");
    lua_pop(L, 1);
    if (lua_getfield(L, 1, "detail") != LUA_TTABLE) {
        return luaL_error(L, "A navigation split names the area of its detail with \"detail\", such as {gui = frame, id = \"detail\"}.");
    }

    lua::Table::checkFields(L, -1, {kRegionFields});
    Region detail = readRegion(L, -1, "The \"detail\" area of a navigation split");
    lua_pop(L, 1);
    lua_getfield(L, 1, "root");
    std::shared_ptr<Screen> root = readScreen(L, -1);
    lua_pop(L, 1);
    NavigationSplit::Factory placeholder;
    if (lua_getfield(L, 1, "placeholder") != LUA_TNIL) {
        placeholder = readFactory(L, -1, "placeholder");
    }

    lua_pop(L, 1);
    bool collapsed = false;
    lua::Table::readField(L, 1, "collapsed", collapsed);
    const int owner = pushOwner(L, 1);
    const std::shared_ptr<NavigationSplit> split = getNavigation(L).createSplit(std::move(primary), std::move(detail), std::move(root), std::move(placeholder), collapsed);
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<NavigationLink>(std::weak_ptr<NavigationSplit>(split)));
    }

    pushObject(L, split);
    return 1;
}

int NavigationLua::pushHandler(lua_State* L, int options, const char* name) {
    if (lua_getfield(L, options, name) == LUA_TNIL) {
        lua_pop(L, 1);
        return 0;
    }

    luaL_checktype(L, -1, LUA_TFUNCTION);
    return lua_gettop(L);
}

// A detent is "medium", "large", "fit" or a height in UI units, listed from the lowest to the highest.
std::vector<Presentation::Detent> NavigationLua::readDetents(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    std::vector<Presentation::Detent> detents;
    const auto count = static_cast<int>(lua_rawlen(L, index));
    for (int position = 1; position <= count; ++position) {
        lua_rawgeti(L, index, position);
        if (lua_type(L, -1) == LUA_TNUMBER) {
            detents.push_back({.kind = Presentation::Detent::Kind::Height, .height = static_cast<float>(lua_tonumber(L, -1))});
            lua_pop(L, 1);
            continue;
        }

        const std::string_view name = lua_type(L, -1) == LUA_TSTRING ? lua::Stack::read<std::string_view>(L, -1) : std::string_view{};
        const auto found = std::ranges::find(Presentation::kDetentNames, name, &std::pair<std::string_view, Presentation::Detent::Kind>::first);
        if (found == Presentation::kDetentNames.end()) {
            luaL_error(L, "A detent is \"medium\", \"large\", \"fit\" or a height in UI units, not %s.", luaL_tolstring(L, -1, nullptr));
        }

        detents.push_back({.kind = found->second});
        lua_pop(L, 1);
    }

    return detents;
}

void NavigationLua::readPresentOptions(lua_State* L, int index, Presentation::Options& options) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kPresentFields});
    lua::Table::readField(L, index, "style", options.style);
    lua::Table::readField(L, index, "dismissible", options.dismissible);
    lua::Table::readField(L, index, "modal", options.modal);
    lua::Table::readField(L, index, "maxWidth", options.maxWidth);
    lua::Table::readField(L, index, "maxHeight", options.maxHeight);
    lua::Table::readField(L, index, "size", options.size);
    if (lua_getfield(L, index, "color") != LUA_TNIL) {
        options.color = lua::Stack::read<math::Color>(L, -1);
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
        luaL_checktype(L, -1, LUA_TTABLE);
        lua::Table::checkFields(L, -1, {kRegionFields});
        options.anchor = readRegion(L, -1, "The anchor of a popover");
    }

    lua_pop(L, 1);
}

bool NavigationLua::hasLeft(const Navigation& navigation, const Presentation& presentation) {
    const std::vector<std::shared_ptr<Presentation>>& shown = navigation.getPresentations();
    return presentation.isDismissed() && std::ranges::none_of(shown, [&presentation](const std::shared_ptr<Presentation>& entry) { return entry.get() == &presentation; });
}

bool NavigationLua::hasLeft(const Navigation& navigation, const Message& message) {
    const std::vector<std::shared_ptr<Message>>& shown = navigation.getMessages();
    return message.isDismissed() && std::ranges::none_of(shown, [&message](const std::shared_ptr<Message>& entry) { return entry.get() == &message; });
}

core::Json NavigationLua::getOutcome(const Presentation& presentation) {
    return presentation.getResult();
}

core::Json NavigationLua::getOutcome(const Message& message) {
    return std::string(Message::reasonName(*message.getReason()));
}

// The promise of the result lives in the userdata, so it lasts while Lua holds the object, and a userdata pushed again for an object that left makes a promise that already holds its result.
template <typename T> void NavigationLua::pushResult(lua_State* L, int index) {
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

void NavigationLua::finishPresentation(lua_State* L, const std::shared_ptr<Presentation>& presentation, int owner, int dismissHandler, int detentHandler) {
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<NavigationLink>(std::weak_ptr<Presentation>(presentation)));
    }

    if (dismissHandler != 0) {
        auto function = std::make_shared<lua::Reference>(L, dismissHandler);
        // clang-format off
        core::Connection connection = presentation->dismissed.connect([function](Presentation& left) {
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
        core::Connection connection = presentation->detentChanged.connect([function](Presentation& moved) {
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
    pushResult<Presentation>(L, -1);
    lua_pop(L, 1);
}

// Presents a screen with `ui.present(screen, {style = 'sheet', detents = {'medium', 'large'}, transition = {...}, dismissible = true, owner = scene, onDismiss = function(event) end})`.
int NavigationLua::present(lua_State* L) {
    std::shared_ptr<Screen> screen = readScreen(L, 1);
    Presentation::Options options;
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

    const std::shared_ptr<Presentation> presentation = getNavigation(L).present(std::move(screen), std::move(options));
    finishPresentation(L, presentation, owner, dismissHandler, detentHandler);
    return 1;
}

// Presents an alert with `ui.presentAlert{style = 'alert', title = 'Delete?', message = '...', actions = {{id = 'cancel', text = 'Cancel', role = 'cancel'}, {id = 'delete', text = 'Delete', role = 'destructive'}}, anchor = {gui = gui, id = 'share'}, owner = scene, onDismiss = function(event) end}`.
int NavigationLua::presentAlert(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kAlertFields});
    Navigation::Alert alert;
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
        luaL_checktype(L, -1, LUA_TTABLE);
        lua::Table::checkFields(L, -1, {kActionFields});
        Navigation::Alert::Action action;
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
        luaL_checktype(L, -1, LUA_TTABLE);
        lua::Table::checkFields(L, -1, {kRegionFields});
        alert.anchor = readRegion(L, -1, "The anchor of an action sheet");
    }

    lua_pop(L, 1);
    const int dismissHandler = pushHandler(L, 1, "onDismiss");
    const int owner = pushOwner(L, 1);
    const std::shared_ptr<Presentation> presentation = getNavigation(L).presentAlert(std::move(alert));
    finishPresentation(L, presentation, owner, dismissHandler, 0);
    return 1;
}

core::Json NavigationLua::readMessageProperties(lua_State* L, int index, std::span<const std::string_view> keys) {
    const int table = lua_absindex(L, index);
    core::Json properties = core::Json::object();
    for (const std::string_view key : keys) {
        lua_pushlstring(L, key.data(), key.size());
        if (lua_gettable(L, table) != LUA_TNIL) {
            properties[std::string(key)] = lua::JsonConverter::read(L, -1);
        }

        lua_pop(L, 1);
    }

    return properties;
}

void NavigationLua::connectMessage(lua_State* L, core::Signal<Message&>& signal, int handler, int owner) {
    if (handler == 0) {
        return;
    }

    auto function = std::make_shared<lua::Reference>(L, handler);
    // clang-format off
    core::Connection connection = signal.connect([function](Message& message) {
        lua_State* main = function->getState();
        lua::Runtime::runReporting(main, [&] {
            function->push(main);
            lua_createtable(main, 0, 2);
            pushObject(main, message.shared_from_this());
            lua_setfield(main, -2, "message");
            if (message.getReason()) {
                lua::Stack::push(main, Message::reasonName(*message.getReason()));
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
int NavigationLua::showMessage(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {Message::Content::kContentKeys, Message::Content::kPlacementKeys, kMessageHandlerFields});
    core::Json properties = readMessageProperties(L, 1, Message::Content::kPlacementKeys);
    properties.update(readMessageProperties(L, 1, Message::Content::kContentKeys));
    const int showHandler = pushHandler(L, 1, "onShow");
    const int actionHandler = pushHandler(L, 1, "onAction");
    const int tapHandler = pushHandler(L, 1, "onTap");
    const int dismissHandler = pushHandler(L, 1, "onDismiss");
    const int owner = pushOwner(L, 1);

    const std::shared_ptr<Message> message = getNavigation(L).showMessage(properties);
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<NavigationLink>(std::weak_ptr<Message>(message)));
    }

    connectMessage(L, message->appeared, showHandler, owner);
    connectMessage(L, message->actionPressed, actionHandler, owner);
    connectMessage(L, message->tapped, tapHandler, owner);
    connectMessage(L, message->dismissed, dismissHandler, owner);
    pushObject(L, message);
    pushResult<Message>(L, -1);
    lua_pop(L, 1);
    return 1;
}

// A route reads `{name = 'artist', path = 'artist/{id}', query = {tab = 'string'}, tab = 'search', parent = 'search', detail = false, present = {style = 'sheet'}, screen = function(parameters) return Artist(parameters.id) end}`.
Router::Route NavigationLua::readRoute(lua_State* L, int index) {
    const int table = lua_absindex(L, index);
    luaL_checktype(L, table, LUA_TTABLE);
    lua::Table::checkFields(L, table, {kRouteFields});
    Router::Route route;
    lua::Table::readField(L, table, "name", route.name);
    lua::Table::readField(L, table, "path", route.path);
    lua::Table::readField(L, table, "tab", route.tab);
    lua::Table::readField(L, table, "parent", route.parent);
    lua::Table::readField(L, table, "detail", route.detail);
    if (lua_getfield(L, table, "query") != LUA_TNIL) {
        luaL_checktype(L, -1, LUA_TTABLE);
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            const std::string name = lua::Stack::read<std::string>(L, -2);
            Router::Type type = Router::Type::String;
            lua::Table::readValue(L, "query", type);
            route.query.emplace_back(name, type);
            lua_pop(L, 1);
        }

        std::ranges::sort(route.query, {}, &std::pair<std::string, Router::Type>::first);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, table, "present") != LUA_TNIL) {
        Presentation::Options presentation;
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
        route.screen = [function](const core::Json& parameters) {
            std::shared_ptr<Screen> made;
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
int NavigationLua::router(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kRouterFields});
    Router::Options options;
    if (lua_getfield(L, 1, "tabs") != LUA_TNIL) {
        options.tabs = lua::Userdata::checkShared<NavigationTabs>(L, -1);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, 1, "stack") != LUA_TNIL) {
        options.stack = lua::Userdata::checkShared<NavigationStack>(L, -1);
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
    const std::shared_ptr<Router> created = getNavigation(L).createRouter(std::move(options));
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<NavigationLink>(std::weak_ptr<Router>(created)));
    }

    if (handler != 0) {
        auto function = std::make_shared<lua::Reference>(L, handler);
        // clang-format off
        core::Connection connection = created->unmatched.connect([function](const std::string& url) {
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

int NavigationLua::routerOpen(lua_State* L) {
    Router& router = lua::Userdata::check<Router>(L, 1);
    const std::string link = lua::Stack::read<std::string>(L, 2);
    router.open(link, readTransition(L, 3, {}));
    return 0;
}

int NavigationLua::routerPush(lua_State* L) {
    Router& router = lua::Userdata::check<Router>(L, 1);
    const std::string location = lua::Stack::read<std::string>(L, 2);
    router.push(location, readTransition(L, 3, {}));
    return 0;
}

// Returns the name of the route a link matches and its parameters, or nothing.
int NavigationLua::routerMatch(lua_State* L) {
    const Router& router = lua::Userdata::check<Router>(L, 1);
    const std::optional<Router::Match> found = router.match(lua::Stack::read<std::string_view>(L, 2));
    if (!found) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, found->route->name);
    lua::JsonConverter::push(L, found->parameters);
    return 2;
}

int NavigationLua::routerLocation(lua_State* L) {
    const Router& router = lua::Userdata::check<Router>(L, 1);
    const std::shared_ptr<Screen> screen = readScreen(L, 2);
    const std::string location = router.getLocation(*screen);
    if (location.empty()) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, location);
    return 1;
}

int NavigationLua::routerSave(lua_State* L) {
    lua::JsonConverter::push(L, lua::Userdata::check<Router>(L, 1).save());
    return 1;
}

int NavigationLua::routerRestore(lua_State* L) {
    Router& router = lua::Userdata::check<Router>(L, 1);
    router.restore(lua::JsonConverter::read(L, 2));
    return 0;
}

int NavigationLua::routerClose(lua_State* L) {
    lua::Userdata::check<Router>(L, 1).close();
    return 0;
}

int NavigationLua::routerClosed(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<Router>(L, 1).isClosed() ? 1 : 0);
    return 1;
}

int NavigationLua::goBack(lua_State* L) {
    lua_pushboolean(L, getNavigation(L).goBack() ? 1 : 0);
    return 1;
}

int NavigationLua::canGoBack(lua_State* L) {
    lua_pushboolean(L, getNavigation(L).canGoBack() ? 1 : 0);
    return 1;
}

int NavigationLua::screenState(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "A screen is a table with a \"build\" method, not %s.", luaL_typename(L, 1));
    }

    lua::Stack::push(L, lua::EnumNames<Screen::State>::name(ScriptedScreen::findState(L, 1)));
    return 1;
}

int NavigationLua::stackPush(lua_State* L) {
    NavigationStack& stack = lua::Userdata::check<NavigationStack>(L, 1);
    std::shared_ptr<Screen> screen = readScreen(L, 2);
    stack.push(std::move(screen), readTransition(L, 3, {}));
    return 0;
}

int NavigationLua::stackPop(lua_State* L) {
    lua::Userdata::check<NavigationStack>(L, 1).pop(readTransition(L, 2, {}));
    return 0;
}

int NavigationLua::stackPopTo(lua_State* L) {
    NavigationStack& stack = lua::Userdata::check<NavigationStack>(L, 1);
    const std::shared_ptr<Screen> screen = readScreen(L, 2);
    stack.popTo(*screen, readTransition(L, 3, {}));
    return 0;
}

int NavigationLua::stackPopToRoot(lua_State* L) {
    lua::Userdata::check<NavigationStack>(L, 1).popToRoot(readTransition(L, 2, {}));
    return 0;
}

int NavigationLua::stackReplace(lua_State* L) {
    NavigationStack& stack = lua::Userdata::check<NavigationStack>(L, 1);
    std::shared_ptr<Screen> screen = readScreen(L, 2);
    stack.replace(std::move(screen), readTransition(L, 3, {}));
    return 0;
}

int NavigationLua::stackSetScreens(lua_State* L) {
    NavigationStack& stack = lua::Userdata::check<NavigationStack>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<std::shared_ptr<Screen>> next;
    const auto count = static_cast<int>(lua_rawlen(L, 2));
    for (int position = 1; position <= count; ++position) {
        lua_rawgeti(L, 2, position);
        next.push_back(readScreen(L, -1));
        lua_pop(L, 1);
    }

    stack.setScreens(std::move(next), readTransition(L, 3, {}));
    return 0;
}

int NavigationLua::stackClose(lua_State* L) {
    lua::Userdata::check<NavigationStack>(L, 1).close();
    return 0;
}

int NavigationLua::stackScreens(lua_State* L) {
    const std::vector<std::shared_ptr<Screen>>& screens = lua::Userdata::check<NavigationStack>(L, 1).getScreens();
    lua_createtable(L, static_cast<int>(screens.size()), 0);
    for (std::size_t position = 0; position < screens.size(); ++position) {
        ScriptedScreen::push(L, screens[position]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(position + 1));
    }

    return 1;
}

int NavigationLua::stackTop(lua_State* L) {
    ScriptedScreen::push(L, lua::Userdata::check<NavigationStack>(L, 1).getTop());
    return 1;
}

int NavigationLua::stackSize(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(lua::Userdata::check<NavigationStack>(L, 1).size()));
    return 1;
}

int NavigationLua::stackCanGoBack(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<NavigationStack>(L, 1).canGoBack() ? 1 : 0);
    return 1;
}

int NavigationLua::stackTransitioning(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<NavigationStack>(L, 1).isTransitioning() ? 1 : 0);
    return 1;
}

int NavigationLua::stackShown(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<NavigationStack>(L, 1).isShown() ? 1 : 0);
    return 1;
}

int NavigationLua::stackSetShown(lua_State* L) {
    lua::Userdata::check<NavigationStack>(L, 1).setShown(lua::Stack::read<bool>(L, 3));
    return 0;
}

int NavigationLua::stackClosed(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<NavigationStack>(L, 1).isClosed() ? 1 : 0);
    return 1;
}

int NavigationLua::tabsSelect(lua_State* L) {
    lua::Userdata::check<NavigationTabs>(L, 1).select(lua::Stack::read<std::string_view>(L, 2));
    return 0;
}

int NavigationLua::tabsStack(lua_State* L) {
    pushStack(L, lua::Userdata::check<NavigationTabs>(L, 1).getStack(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int NavigationLua::tabsClose(lua_State* L) {
    lua::Userdata::check<NavigationTabs>(L, 1).close();
    return 0;
}

int NavigationLua::tabsSelected(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<NavigationTabs>(L, 1).getSelected());
    return 1;
}

int NavigationLua::tabsCurrent(lua_State* L) {
    NavigationTabs& tabs = lua::Userdata::check<NavigationTabs>(L, 1);
    if (tabs.isClosed()) {
        lua_pushnil(L);
        return 1;
    }

    pushStack(L, tabs.getCurrent());
    return 1;
}

int NavigationLua::tabsClosed(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<NavigationTabs>(L, 1).isClosed() ? 1 : 0);
    return 1;
}

int NavigationLua::splitShowDetail(lua_State* L) {
    NavigationSplit& split = lua::Userdata::check<NavigationSplit>(L, 1);
    std::shared_ptr<Screen> screen = readScreen(L, 2);
    const std::optional<NavigationTransition> transition = lua_isnoneornil(L, 3) ? std::nullopt : std::optional(readTransition(L, 3, {}));
    split.showDetail(std::move(screen), transition);
    return 0;
}

int NavigationLua::splitClearDetail(lua_State* L) {
    NavigationSplit& split = lua::Userdata::check<NavigationSplit>(L, 1);
    const std::optional<NavigationTransition> transition = lua_isnoneornil(L, 2) ? std::nullopt : std::optional(readTransition(L, 2, {}));
    split.clearDetail(transition);
    return 0;
}

int NavigationLua::splitClose(lua_State* L) {
    lua::Userdata::check<NavigationSplit>(L, 1).close();
    return 0;
}

int NavigationLua::splitCollapsed(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<NavigationSplit>(L, 1).isCollapsed() ? 1 : 0);
    return 1;
}

int NavigationLua::splitSetCollapsed(lua_State* L) {
    lua::Userdata::check<NavigationSplit>(L, 1).setCollapsed(lua::Stack::read<bool>(L, 3));
    return 0;
}

int NavigationLua::splitHasDetail(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<NavigationSplit>(L, 1).hasDetail() ? 1 : 0);
    return 1;
}

int NavigationLua::splitPrimary(lua_State* L) {
    pushStack(L, lua::Userdata::check<NavigationSplit>(L, 1).getPrimary());
    return 1;
}

int NavigationLua::splitDetail(lua_State* L) {
    pushStack(L, lua::Userdata::check<NavigationSplit>(L, 1).getDetail());
    return 1;
}

int NavigationLua::splitClosed(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<NavigationSplit>(L, 1).isClosed() ? 1 : 0);
    return 1;
}

// Dismisses with `presentation:dismiss{result = value, transition = {...}}`, where the result is a value that JSON holds.
int NavigationLua::presentationDismiss(lua_State* L) {
    Presentation& presentation = lua::Userdata::check<Presentation>(L, 1);
    core::Json result;
    std::optional<NavigationTransition> transition;
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

int NavigationLua::presentationScreen(lua_State* L) {
    ScriptedScreen::push(L, lua::Userdata::check<Presentation>(L, 1).getScreen());
    return 1;
}

int NavigationLua::presentationStyle(lua_State* L) {
    lua::Stack::push(L, Presentation::styleName(lua::Userdata::check<Presentation>(L, 1).getStyle()));
    return 1;
}

int NavigationLua::presentationDismissible(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<Presentation>(L, 1).isDismissible() ? 1 : 0);
    return 1;
}

int NavigationLua::presentationDismissed(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<Presentation>(L, 1).isDismissed() ? 1 : 0);
    return 1;
}

int NavigationLua::presentationModal(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<Presentation>(L, 1).isModal() ? 1 : 0);
    return 1;
}

int NavigationLua::presentationResult(lua_State* L) {
    static_cast<void>(lua::Userdata::check<Presentation>(L, 1));
    pushResult<Presentation>(L, 1);
    return 1;
}

int NavigationLua::presentationDetent(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(lua::Userdata::check<Presentation>(L, 1).getDetent() + 1));
    return 1;
}

int NavigationLua::presentationSetDetent(lua_State* L) {
    Presentation& presentation = lua::Userdata::check<Presentation>(L, 1);
    const lua_Integer position = luaL_checkinteger(L, 3);
    const auto count = static_cast<lua_Integer>(presentation.getOptions().detents.size());
    if (position < 1 || position > count) {
        return luaL_error(L, "A sheet rests at the place of one of its detents, from 1 to %d.", static_cast<int>(count));
    }

    presentation.setDetent(static_cast<std::size_t>(position - 1));
    return 0;
}

// Changes a message with `message:update{text = 'Uploaded.', tone = 'success', indicator = 'icon', duration = 2}`.
int NavigationLua::messageUpdate(lua_State* L) {
    Message& message = lua::Userdata::check<Message>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {Message::Content::kContentKeys});
    message.update(readMessageProperties(L, 2, Message::Content::kContentKeys));
    return 0;
}

int NavigationLua::messageDismiss(lua_State* L) {
    lua::Userdata::check<Message>(L, 1).dismiss();
    return 0;
}

int NavigationLua::messageStyle(lua_State* L) {
    lua::Stack::push(L, Message::styleName(lua::Userdata::check<Message>(L, 1).getStyle()));
    return 1;
}

int NavigationLua::messageShown(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<Message>(L, 1).isShown() ? 1 : 0);
    return 1;
}

int NavigationLua::messageDismissed(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<Message>(L, 1).isDismissed() ? 1 : 0);
    return 1;
}

int NavigationLua::messageResult(lua_State* L) {
    static_cast<void>(lua::Userdata::check<Message>(L, 1));
    pushResult<Message>(L, 1);
    return 1;
}

void NavigationLua::addTo(lua_State* L, int module) {
    const int target = lua_absindex(L, module);
    const luaL_Reg functions[] = {
        {"navigationStack", &lua::Binding::native<&navigationStack>}, {"navigationTabs", &lua::Binding::native<&navigationTabs>}, {"navigationSplit", &lua::Binding::native<&navigationSplit>}, {"present", &lua::Binding::native<&present>}, {"presentAlert", &lua::Binding::native<&presentAlert>}, {"showMessage", &lua::Binding::native<&showMessage>}, {"router", &lua::Binding::native<&router>}, {"goBack", &lua::Binding::native<&goBack>}, {"canGoBack", &lua::Binding::native<&canGoBack>}, {"screenState", &lua::Binding::native<&screenState>}, {nullptr, nullptr},
    };
    for (const luaL_Reg* entry = functions; entry->name != nullptr; ++entry) {
        lua_pushcfunction(L, entry->func);
        lua_setfield(L, target, entry->name);
    }

    // The `Screen` base class names the screens that apps build with `haylen.class`.
    core::ClassLua::push(L);
    lua_pushliteral(L, "Screen");
    lua_call(L, 1, 1);
    lua_setfield(L, target, "Screen");
}

void NavigationLua::install(lua_State* L) {
    lua::ClassBuilder<NavigationStack>(L).function("push", &lua::Binding::native<&stackPush>).function("pop", &lua::Binding::native<&stackPop>).function("popTo", &lua::Binding::native<&stackPopTo>).function("popToRoot", &lua::Binding::native<&stackPopToRoot>).function("replace", &lua::Binding::native<&stackReplace>).function("setScreens", &lua::Binding::native<&stackSetScreens>).function("close", &lua::Binding::native<&stackClose>).property("screens", &stackScreens).property("top", &stackTop).property("size", &stackSize).property("canGoBack", &stackCanGoBack).property("transitioning", &stackTransitioning).property("shown", &stackShown, &lua::Binding::native<&stackSetShown>).property("closed", &stackClosed).install();
    lua::ClassBuilder<NavigationTabs>(L).function("select", &lua::Binding::native<&tabsSelect>).function("stack", &lua::Binding::native<&tabsStack>).function("close", &lua::Binding::native<&tabsClose>).property("selected", &tabsSelected).property("current", &lua::Binding::native<&tabsCurrent>).property("closed", &tabsClosed).install();
    lua::ClassBuilder<NavigationSplit>(L).function("showDetail", &lua::Binding::native<&splitShowDetail>).function("clearDetail", &lua::Binding::native<&splitClearDetail>).function("close", &lua::Binding::native<&splitClose>).property("collapsed", &splitCollapsed, &lua::Binding::native<&splitSetCollapsed>).property("hasDetail", &splitHasDetail).property("primary", &splitPrimary).property("detail", &splitDetail).property("closed", &splitClosed).install();
    lua::ClassBuilder<Presentation>(L).function("dismiss", &lua::Binding::native<&presentationDismiss>).property("screen", &presentationScreen).property("style", &presentationStyle).property("dismissible", &presentationDismissible).property("dismissed", &presentationDismissed).property("modal", &presentationModal).property("result", &lua::Binding::native<&presentationResult>).property("detent", &presentationDetent, &lua::Binding::native<&presentationSetDetent>).install();
    lua::ClassBuilder<Message>(L).function("update", &lua::Binding::native<&messageUpdate>).function("dismiss", &lua::Binding::native<&messageDismiss>).property("style", &messageStyle).property("shown", &messageShown).property("dismissed", &messageDismissed).property("result", &lua::Binding::native<&messageResult>).install();
    lua::ClassBuilder<Router>(L).function("open", &lua::Binding::native<&routerOpen>).function("push", &lua::Binding::native<&routerPush>).function("match", &lua::Binding::native<&routerMatch>).function("location", &lua::Binding::native<&routerLocation>).function("save", &lua::Binding::native<&routerSave>).function("restore", &lua::Binding::native<&routerRestore>).function("close", &lua::Binding::native<&routerClose>).property("closed", &routerClosed).install();
}

} // namespace haylen::ui
