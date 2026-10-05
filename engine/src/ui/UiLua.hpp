#pragma once

#include <array>
#include <cstddef>
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

class Gui;
struct Event;

// Installs `haylen.ui`, which mounts GUIs of components built from Lua tables, and the `Gui` class.
class UiLua final {
  public:
    static void install(lua_State* L);

    // Calls the Lua handler a GUI declared for the event, when it has one.
    static void deliverEvent(lua_State* L, Gui& gui, const Event& event);

    // Drops the handlers of a GUI that is no longer mounted and ends everything its userdata owns.
    static void forgetGui(lua_State* L, const Gui& gui);

  private:
    class StackScope;

    // The registry keeps one entry per mounted GUI, keyed by the GUI address: its userdata and the handlers of its nodes by id and event name.
    static constexpr const char* kHandlersKey = "haylen.ui.handlers";
    static constexpr std::array<std::string_view, 3> kMountFields{"placement", "layer", "owner"};
    static constexpr std::array<std::string_view, 1> kEventFields{"owner"};

    [[nodiscard]] static plugins::UiPlugin& getPlugin(lua_State* L);
    static void pushRoot(lua_State* L);
    [[nodiscard]] static std::optional<std::string> handlerEvent(lua_State* L, int key, int value);
    static void storeHandler(lua_State* L, int handlers, const std::string& id, const std::string& event, int value);
    [[nodiscard]] static std::string nextGeneratedId(lua_State* L);
    [[nodiscard]] static core::Json convertNode(lua_State* L, int index, int handlers, std::size_t depth, std::size_t& count);
    [[nodiscard]] static core::Json convertProperties(lua_State* L, int index, int collected);
    static void collectIds(const core::Json& node, std::vector<std::string>& ids);
    [[nodiscard]] static bool pushHandlers(lua_State* L, const Gui& gui);
    static void pruneHandlers(lua_State* L, int handlers, const Gui& gui);
    static void listenToHandlers(lua_State* L, Gui& gui, int handlers);
    static void listenToEvents(lua_State* L, Gui& gui, std::string_view id, int events);
    static void pushEventTable(lua_State* L, const Gui& gui, const Event& event);
    static void pushGui(lua_State* L, const std::shared_ptr<Gui>& gui);
    [[nodiscard]] static Gui& checkGui(lua_State* L);

    static int mount(lua_State* L);
    static int guiSet(lua_State* L);
    static int guiReplaceChildren(lua_State* L);
    static int guiRemoveHandler(lua_State* L);
    static int guiGet(lua_State* L);
    static int guiBounds(lua_State* L);
    static int guiHas(lua_State* L);
    static int guiCommand(lua_State* L);
    static int guiUnmount(lua_State* L);
    static int guiVisible(lua_State* L);
    static int guiSetVisible(lua_State* L);
    static int guiMounted(lua_State* L);
    static int guiPlacement(lua_State* L);
    static int guiTransform(lua_State* L);
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
    static int themeImageFilter(lua_State* L);
    static int themeSurface(lua_State* L);
    static int onEvent(lua_State* L);
    static int addFont(lua_State* L);
    static int usingPointer(lua_State* L);
    static int usingKeyboard(lua_State* L);
    static int focused(lua_State* L);
    static int focusOwner(lua_State* L);
    static int clearFocus(lua_State* L);
    static int focusRingVisible(lua_State* L);
    static int safeAreaVisible(lua_State* L);
    static int setSafeAreaVisible(lua_State* L);
    static int setDirection(lua_State* L);
    static int setScaleMode(lua_State* L);
    static int scaleMode(lua_State* L);
    static int setScale(lua_State* L);
    static int scale(lua_State* L);
    static int direction(lua_State* L);
    static int kinds(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::ui
