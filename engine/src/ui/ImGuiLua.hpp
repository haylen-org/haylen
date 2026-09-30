#pragma once

#include <array>
#include <string_view>

#include <imgui.h>

struct lua_State;

namespace haylen::ui {

class Backend;

// Installs `haylen.imgui`, which draws immediate mode Dear ImGui windows and widgets from Lua for tools and debug panels.
class ImGuiLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 9> kWindowFields{"x", "y", "width", "height", "closable", "noTitleBar", "noResize", "noMove", "autoResize"};

    static Backend& requireFrame(lua_State* L);
    [[nodiscard]] static const char* readLabel(lua_State* L, int index);
    [[nodiscard]] static ImVec2 readOptionalSize(lua_State* L, int first);
    [[nodiscard]] static const char* readNumberFormat(lua_State* L, int index);
    static int returnChanged(lua_State* L, bool changed, int values);
    static int plot(lua_State* L, bool histogram);

    static int beginWindow(lua_State* L);
    static int endWindow(lua_State* L);
    static int text(lua_State* L);
    static int textColored(lua_State* L);
    static int textWrapped(lua_State* L);
    static int button(lua_State* L);
    static int checkbox(lua_State* L);
    static int sliderFloat(lua_State* L);
    static int sliderInt(lua_State* L);
    static int dragFloat(lua_State* L);
    static int inputText(lua_State* L);
    static int inputFloat(lua_State* L);
    static int inputInt(lua_State* L);
    static int colorEdit(lua_State* L);
    static int combo(lua_State* L);
    static int selectable(lua_State* L);
    static int treeNode(lua_State* L);
    static int treePop(lua_State* L);
    static int collapsingHeader(lua_State* L);
    static int separator(lua_State* L);
    static int sameLine(lua_State* L);
    static int spacing(lua_State* L);
    static int dummy(lua_State* L);
    static int beginChild(lua_State* L);
    static int endChild(lua_State* L);
    static int beginTabBar(lua_State* L);
    static int endTabBar(lua_State* L);
    static int beginTabItem(lua_State* L);
    static int endTabItem(lua_State* L);
    static int beginTable(lua_State* L);
    static int endTable(lua_State* L);
    static int tableSetupColumn(lua_State* L);
    static int tableHeadersRow(lua_State* L);
    static int tableNextRow(lua_State* L);
    static int tableNextColumn(lua_State* L);
    static int progressBar(lua_State* L);
    static int plotLines(lua_State* L);
    static int plotHistogram(lua_State* L);
    static int image(lua_State* L);
    static int setNextWindowPos(lua_State* L);
    static int setNextWindowSize(lua_State* L);
    static int setNextItemWidth(lua_State* L);
    static int isItemHovered(lua_State* L);
    static int setTooltip(lua_State* L);
    static int openPopup(lua_State* L);
    static int beginPopup(lua_State* L);
    static int endPopup(lua_State* L);
    static int closeCurrentPopup(lua_State* L);
    static int pushId(lua_State* L);
    static int popId(lua_State* L);
    static int pushFont(lua_State* L);
    static int popFont(lua_State* L);
    static int showDemoWindow(lua_State* L);
    static int showMetricsWindow(lua_State* L);
    static int framerate(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::ui
