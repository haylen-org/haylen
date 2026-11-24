#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/ui/CollectionIds.hpp"
#include "haylen/ui/CollectionSource.hpp"

struct lua_State;

namespace haylen::ui {
class Collection;
}

namespace haylen::ui {
class Gui;
}

namespace haylen::lua::ui {
// The items of the Lua list that `collection:setItems` gives a collection. The GUI keeps the list for the collection, and the source caches the id and the type of every item, so binding reads only the fields it binds, with raw table access that runs no Lua code. The methods that change the items change the list too, so the app and the collection always see the same items, and they check only the ids they add, so a change costs the items it touches and the shift of the list, as `table.insert` does.
class ScriptedItemSource final : public haylen::ui::CollectionSource {
  public:
    ScriptedItemSource(lua_State* L, const haylen::ui::Gui& owner, haylen::ui::Collection& shown);

    // Reads and checks every item of the list at the index before anything changes, then shows them, compared with the items before by id. The list becomes the one the GUI keeps.
    void assign(lua_State* L, int list);
    void insert(lua_State* L, std::size_t index, int added);
    void remove(lua_State* L, std::size_t index, std::size_t count);
    void move(lua_State* L, std::size_t from, std::size_t to);
    void replace(lua_State* L, std::size_t index, int item);

    // Reads again the id and the type of an item the app changed in place, and binds it again.
    void reload(lua_State* L, std::size_t index);

    // Pushes the table of an item.
    void pushItem(lua_State* L, std::size_t index) const;

    [[nodiscard]] std::size_t getCount() const override {
        return ids.size();
    }
    [[nodiscard]] std::string_view getId(std::size_t index) const override {
        return ids[index];
    }
    [[nodiscard]] std::string_view getType(std::size_t index) const override {
        return typeNames[types[index]];
    }
    [[nodiscard]] haylen::core::Json getValue(std::size_t index, std::string_view field) const override;
    void setValue(std::size_t index, std::string_view field, const haylen::core::Json& value) override;

  private:
    // Reads the id and the type of the item table at an index, which is the item at `position` from 1 in the messages.
    [[nodiscard]] std::pair<std::string, std::uint16_t> readItem(lua_State* L, int table, std::size_t position);
    [[nodiscard]] std::uint16_t internType(std::string_view type);

    void pushList(lua_State* L) const;

    lua_State* state;
    const haylen::ui::Gui& gui;
    haylen::ui::Collection& collection;
    std::vector<std::string> ids;
    haylen::ui::CollectionIds known;
    std::vector<std::uint16_t> types;
    std::vector<std::string> typeNames;
};

} // namespace haylen::lua::ui
