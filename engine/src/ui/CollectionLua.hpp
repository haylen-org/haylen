#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::ui {

class Collection;
class CollectionCell;
class Gui;
class LuaItemSource;

// Installs the `UiCollection` class, the handle `gui:collection(id)` returns, and the `UiCell` class of the cells binders receive. The GUI keeps the Lua values of each collection, its list of items, its binders, its page loader and its pages, in the registry until it unmounts, so a binder that refers to its scene never keeps the scene alive.
class CollectionLua final {
  public:
    // A collection of a GUI, found again by its id at every call, so the handle reads as released once its node is gone.
    struct Handle {
        std::weak_ptr<Gui> gui;
        Collection* collection = nullptr;
        std::string id;
    };

    // A cell while it shows one item, which reads as stale once the cell shows another item.
    struct CellHandle {
        std::weak_ptr<CollectionCell> cell;
        std::uint64_t generation = 0;
        std::string item;
    };

    static void install(lua_State* L);

    // Pushes the handle of a collection, the same one while its node exists.
    static void push(lua_State* L, const std::shared_ptr<Gui>& gui, Collection& collection);

    // Pushes or stores a Lua value the GUI keeps for a collection, such as its list of items, where a value index of 0 stores nil.
    static void pushValue(lua_State* L, const Gui& gui, const Collection& collection, const char* field);
    static void setValue(lua_State* L, const Gui& gui, const Collection& collection, const char* field, int value);
    static void pushPage(lua_State* L, const Gui& gui, const Collection& collection, std::size_t page);
    static void setPage(lua_State* L, const Gui& gui, const Collection& collection, std::size_t page, int list);

    // Reads the id and the type of the item table at an index, the item at `position` from 1 in the messages, and checks them against the types of the collection.
    [[nodiscard]] static std::pair<std::string, std::string> readItem(lua_State* L, int table, std::size_t position, const Collection& collection);

    // Drops the Lua values of the collections of a GUI that unmounted, or of the collections a replacement of its nodes removed.
    static void forget(lua_State* L, const Gui& gui);
    static void prune(lua_State* L, const Gui& gui);

  private:
    static constexpr const char* kValuesKey = "haylen.ui.collections";
    static constexpr std::array<std::string_view, 4> kPageFields{"count", "pageSize", "maxPages", "load"};
    static constexpr std::array<std::string_view, 3> kScrollFields{"align", "offset", "animated"};
    static constexpr std::array<std::string_view, 1> kAnimationFields{"animated"};
    static constexpr std::array<std::string_view, 3> kStateFields{"item", "distance", "focused"};

    // Pushes the table of Lua values of a collection and returns `true`, creating it when asked, or pushes nothing and returns `false`.
    static bool pushEntry(lua_State* L, const Gui& gui, const Collection& collection, bool create);

    [[nodiscard]] static Collection& check(lua_State* L);
    [[nodiscard]] static std::shared_ptr<Gui> checkGui(lua_State* L);
    [[nodiscard]] static std::size_t readTarget(lua_State* L, int index, const Collection& collection);
    [[nodiscard]] static std::size_t readIndex(lua_State* L, int index, const Collection& collection, std::size_t limit);
    static void checkUnbound(lua_State* L, const Collection& collection);
    [[nodiscard]] static LuaItemSource& requireList(lua_State* L, Collection& collection);
    static void pushItem(lua_State* L, const Collection& collection, std::size_t index);
    static void pushCell(lua_State* L, CollectionCell& cell);
    [[nodiscard]] static CollectionCell& checkCell(lua_State* L);
    static void callBinder(lua_State* L, const Gui& gui, const Collection& collection, const std::string& type, CollectionCell& cell, std::size_t index);

    static int setItems(lua_State* L);
    static int setPages(lua_State* L);
    static int insert(lua_State* L);
    static int remove(lua_State* L);
    static int move(lua_State* L);
    static int replace(lua_State* L);
    static int reload(lua_State* L);
    static int setBinder(lua_State* L);
    static int scrollTo(lua_State* L);
    static int scrollBy(lua_State* L);
    static int focus(lua_State* L);
    static int indexOf(lua_State* L);
    static int cellOf(lua_State* L);
    static int visibleRange(lua_State* L);
    static int saveState(lua_State* L);
    static int restoreState(lua_State* L);
    static int count(lua_State* L);
    static int scrollOffset(lua_State* L);
    static int setScrollOffset(lua_State* L);
    static int contentLength(lua_State* L);
    static int viewportLength(lua_State* L);
    static int focusedItem(lua_State* L);
    static int selected(lua_State* L);

    static int cellSet(lua_State* L);
    static int cellTransform(lua_State* L);
    static int cellItem(lua_State* L);
    static int cellIndex(lua_State* L);
    static int cellType(lua_State* L);
    static int cellBound(lua_State* L);
};

} // namespace haylen::ui

namespace haylen::lua {

template <> struct Type<ui::CollectionLua::Handle> {
    static constexpr const char* name = "haylen.UiCollection";
    using Storage = ui::CollectionLua::Handle;
};

template <> struct Type<ui::CollectionLua::CellHandle> {
    static constexpr const char* name = "haylen.UiCell";
    using Storage = ui::CollectionLua::CellHandle;
};

} // namespace haylen::lua
