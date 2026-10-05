#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <lua.hpp>

#include "haylen/core/Json.hpp"
#include "haylen/ui/CollectionSource.hpp"

namespace haylen::ui {

class Collection;
class Gui;

// A number of items that a Lua loader gives in pages, for `collection:setPages`. The loader runs once per page the collection asks for, before the GUIs draw, and returns the list of the page or a promise of it. The GUI keeps the loader and the lists of the pages, at most `maxPages` of them, which drops the pages farthest from the view, and an item of a page that has not arrived has no id, so the collection shows its placeholder type.
class LuaPageSource final : public CollectionSource, public std::enable_shared_from_this<LuaPageSource> {
  public:
    struct Options {
        std::size_t count = 0;
        std::size_t pageSize = 50;
        std::size_t maxPages = 16;
    };

    LuaPageSource(lua_State* L, const Gui& owner, Collection& shown);

    // Takes new options, keeping the pages that still fit when the page size stays.
    void configure(const Options& value);

    // Stores the list a loader gave for a page, unless the page was dropped or loaded again meanwhile.
    void store(lua_State* L, std::size_t page, std::uint64_t ticket, int list);

    void pushItem(lua_State* L, std::size_t index) const;

    [[nodiscard]] std::size_t getCount() const override {
        return options.count;
    }
    [[nodiscard]] std::string_view getId(std::size_t index) const override;
    [[nodiscard]] std::string_view getType(std::size_t index) const override;
    [[nodiscard]] core::Json getValue(std::size_t index, std::string_view field) const override;
    void setValue(std::size_t index, std::string_view field, const core::Json& value) override;
    void request(std::size_t first, std::size_t count) override;

  private:
    struct Page {
        std::vector<std::string> ids;
        std::vector<std::string> types;
        std::uint64_t ticket = 0;
        bool loaded = false;
    };

    static constexpr const char* kRequestType = "haylen.UiPageRequest";

    // A page a promise still brings: the source, which may be gone by then, the page and the ticket of the load.
    struct Request {
        std::weak_ptr<LuaPageSource> source;
        std::size_t page = 0;
        std::uint64_t ticket = 0;
    };

    static int awaitPage(lua_State* L);
    static int finishPage(lua_State* L, int status, lua_KContext context);
    static int collectRequest(lua_State* L);

    [[nodiscard]] std::size_t getPageLength(std::size_t page) const noexcept;
    [[nodiscard]] const Page* findLoaded(std::size_t index) const;
    void load(std::size_t page);
    void dropFarthest(std::size_t firstPage, std::size_t lastPage);
    void drop(std::size_t page);

    lua_State* state;
    const Gui& gui;
    Collection& collection;
    Options options;
    std::map<std::size_t, Page> pages;
    std::uint64_t nextTicket = 1;
};

} // namespace haylen::ui
