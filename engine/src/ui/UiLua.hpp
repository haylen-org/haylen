#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"

struct lua_State;

namespace haylen::plugins {
class UiPlugin;
}

namespace haylen::ui {

class Document;
struct Event;

// Installs haylen.ui, which mounts documents of components built from Lua tables, and the UiDocument class.
class UiLua final {
  public:
    static void install(lua_State* L);

    // Calls the Lua handler a document declared for the event, when it has one.
    static void deliverEvent(lua_State* L, Document& document, const Event& event);

    // Drops the handlers of a document that is no longer mounted and ends everything its userdata owns.
    static void forgetDocument(lua_State* L, const Document& document);

  private:
    class StackScope;

    // The registry keeps one entry per mounted document, keyed by the document address: its userdata and the handlers of its nodes by id and event name.
    static constexpr const char* kHandlersKey = "haylen.ui.handlers";
    static constexpr std::array<std::string_view, 3> kMountFields{"placement", "layer", "owner"};

    [[nodiscard]] static plugins::UiPlugin& getPlugin(lua_State* L);
    static void pushRoot(lua_State* L);
    [[nodiscard]] static std::optional<std::string> handlerEvent(lua_State* L, int key, int value);
    static void storeHandler(lua_State* L, int handlers, const std::string& id, const std::string& event, int value);
    [[nodiscard]] static std::string nextGeneratedId(lua_State* L);
    [[nodiscard]] static core::Json convertNode(lua_State* L, int index, int handlers);
    [[nodiscard]] static core::Json convertProperties(lua_State* L, int index, int collected);
    static void collectIds(const core::Json& node, std::vector<std::string>& ids);
    [[nodiscard]] static bool pushHandlers(lua_State* L, const Document& document);
    static void pruneHandlers(lua_State* L, int handlers, const Document& document);
    static void pushEventTable(lua_State* L, const Document& document, const Event& event);
    static void pushDocument(lua_State* L, const std::shared_ptr<Document>& document);
    [[nodiscard]] static Document& checkDocument(lua_State* L);

    static int mount(lua_State* L);
    static int documentSet(lua_State* L);
    static int documentReplace(lua_State* L);
    static int documentRemoveHandler(lua_State* L);
    static int documentGet(lua_State* L);
    static int documentBounds(lua_State* L);
    static int documentHas(lua_State* L);
    static int documentCommand(lua_State* L);
    static int documentUnmount(lua_State* L);
    static int documentVisible(lua_State* L);
    static int documentSetVisible(lua_State* L);
    static int documentMounted(lua_State* L);
    static int documentPlacement(lua_State* L);
    static int documentTransform(lua_State* L);
    static int node(lua_State* L);
    static int kindBuilder(lua_State* L);
    static int moduleIndex(lua_State* L);
    static int setTheme(lua_State* L);
    static int theme(lua_State* L);
    static int themes(lua_State* L);
    static int loadTheme(lua_State* L);
    static int addTheme(lua_State* L);
    static int themeColor(lua_State* L);
    static int themeMetric(lua_State* L);
    static int themeFont(lua_State* L);
    static int themeSurface(lua_State* L);
    static int onEvent(lua_State* L);
    static int addFont(lua_State* L);
    static int wantsPointer(lua_State* L);
    static int wantsKeyboard(lua_State* L);
    static int focused(lua_State* L);
    static int clearFocus(lua_State* L);
    static int focusRingVisible(lua_State* L);
    static int safeAreaVisible(lua_State* L);
    static int setSafeAreaVisible(lua_State* L);
    static int kinds(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::ui
