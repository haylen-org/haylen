#include "ui/NavigationLua.hpp"

#include <lua.hpp>

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
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/NavigationSplit.hpp"
#include "haylen/ui/NavigationStack.hpp"
#include "haylen/ui/NavigationTabs.hpp"
#include "haylen/ui/Presentation.hpp"
#include "haylen/ui/Screen.hpp"
#include "lua/Owners.hpp"
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

// Presents a screen with `ui.present(screen, {style = 'sheet', transition = {...}, dismissible = true, color = '#FF101014', maxWidth = 760, maxHeight = 1000, owner = scene, onDismiss = function(event) end})`.
int NavigationLua::present(lua_State* L) {
    std::shared_ptr<Screen> screen = readScreen(L, 1);
    Presentation::Options options;
    int handler = 0;
    int owner = 0;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kPresentFields});
        lua::Table::readField(L, 2, "style", options.style);
        lua::Table::readField(L, 2, "dismissible", options.dismissible);
        if (lua_getfield(L, 2, "color") != LUA_TNIL) {
            options.color = lua::Stack::read<math::Color>(L, -1);
        }

        lua_pop(L, 1);
        lua::Table::readField(L, 2, "maxWidth", options.maxWidth);
        lua::Table::readField(L, 2, "maxHeight", options.maxHeight);
        if (lua_getfield(L, 2, "transition") != LUA_TNIL) {
            options.transition = readTransition(L, -1, {});
        }

        lua_pop(L, 1);
        if (lua_getfield(L, 2, "onDismiss") != LUA_TNIL) {
            luaL_checktype(L, -1, LUA_TFUNCTION);
            handler = lua_gettop(L);
        } else {
            lua_pop(L, 1);
        }

        owner = pushOwner(L, 2);
    } else {
        owner = lua::Owners::pushDefault(L);
    }

    Navigation& navigation = getNavigation(L);
    const std::shared_ptr<Presentation> presentation = navigation.present(std::move(screen), options);
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<NavigationLink>(std::weak_ptr<Presentation>(presentation)));
    }

    if (handler != 0) {
        auto function = std::make_shared<lua::Reference>(L, handler);
        // clang-format off
        core::Connection connection = presentation->dismissed.connect([function](Presentation& left) {
            lua_State* main = function->getState();
            lua::Runtime::runReporting(main, [&] {
                function->push(main);
                lua_createtable(main, 0, 1);
                pushPresentation(main, left.shared_from_this());
                lua_setfield(main, -2, "presentation");
                lua::Runtime::protectedCall(main, 1, 0);
            });
        });
        // clang-format on
        if (owner != 0) {
            lua::Owners::add(L, owner, connection);
        }
    }

    pushPresentation(L, presentation);
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

int NavigationLua::presentationDismiss(lua_State* L) {
    Presentation& presentation = lua::Userdata::check<Presentation>(L, 1);
    const std::optional<NavigationTransition> transition = lua_isnoneornil(L, 2) ? std::nullopt : std::optional(readTransition(L, 2, {}));
    presentation.dismiss(transition);
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

void NavigationLua::addTo(lua_State* L, int module) {
    const int target = lua_absindex(L, module);
    const luaL_Reg functions[] = {
        {"navigationStack", &lua::Binding::native<&navigationStack>}, {"navigationTabs", &lua::Binding::native<&navigationTabs>}, {"navigationSplit", &lua::Binding::native<&navigationSplit>}, {"present", &lua::Binding::native<&present>}, {"goBack", &lua::Binding::native<&goBack>}, {"canGoBack", &lua::Binding::native<&canGoBack>}, {"screenState", &lua::Binding::native<&screenState>}, {nullptr, nullptr},
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
    lua::ClassBuilder<Presentation>(L).function("dismiss", &lua::Binding::native<&presentationDismiss>).property("screen", &presentationScreen).property("style", &presentationStyle).property("dismissible", &presentationDismissible).property("dismissed", &presentationDismissed).install();
}

} // namespace haylen::ui
