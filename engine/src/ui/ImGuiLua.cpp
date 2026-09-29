#include "ui/ImGuiLua.hpp"

#include <array>
#include <cfloat>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Backend.hpp"

namespace haylen::ui {

// Every call happens between the frame start and the UI render, which is where scenes update and draw.
Backend& ImGuiLua::requireFrame(lua_State* L) {
    Backend& backend = lua::Runtime::getEngine(L).getPlugin<plugins::UiPlugin>().getBackend();
    if (!backend.isFrameActive()) {
        luaL_error(L, "haylen.imgui can only be used while a frame is running.");
    }
    backend.makeCurrent();
    return backend;
}

const char* ImGuiLua::readLabel(lua_State* L, int index) {
    return luaL_checkstring(L, index);
}

ImVec2 ImGuiLua::readOptionalSize(lua_State* L, int first) {
    return {static_cast<float>(luaL_optnumber(L, first, 0.0)), static_cast<float>(luaL_optnumber(L, first + 1, 0.0))};
}

int ImGuiLua::returnChanged(lua_State* L, bool changed, int values) {
    lua_pushboolean(L, changed ? 1 : 0);
    lua_insert(L, -1 - values);
    return values + 1;
}

// Opens a window with beginWindow(name[, {x, y, width, height, closable, noTitleBar, noResize, noMove, autoResize}]) and returns whether it is visible and, for closable windows, whether it is still open.
int ImGuiLua::beginWindow(lua_State* L) {
    (void)requireFrame(L);
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    bool closable = false;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kWindowFields});
        std::optional<float> x;
        std::optional<float> y;
        std::optional<float> width;
        std::optional<float> height;
        bool noTitleBar = false;
        bool noResize = false;
        bool noMove = false;
        bool autoResize = false;
        lua::Table::readField(L, 2, "x", x);
        lua::Table::readField(L, 2, "y", y);
        lua::Table::readField(L, 2, "width", width);
        lua::Table::readField(L, 2, "height", height);
        lua::Table::readField(L, 2, "closable", closable);
        lua::Table::readField(L, 2, "noTitleBar", noTitleBar);
        lua::Table::readField(L, 2, "noResize", noResize);
        lua::Table::readField(L, 2, "noMove", noMove);
        lua::Table::readField(L, 2, "autoResize", autoResize);
        if (x && y) {
            ImGui::SetNextWindowPos({*x, *y}, ImGuiCond_FirstUseEver);
        }
        if (width && height) {
            ImGui::SetNextWindowSize({*width, *height}, ImGuiCond_FirstUseEver);
        }
        flags |= noTitleBar ? ImGuiWindowFlags_NoTitleBar : ImGuiWindowFlags_None;
        flags |= noResize ? ImGuiWindowFlags_NoResize : ImGuiWindowFlags_None;
        flags |= noMove ? ImGuiWindowFlags_NoMove : ImGuiWindowFlags_None;
        flags |= autoResize ? ImGuiWindowFlags_AlwaysAutoResize : ImGuiWindowFlags_None;
    }
    bool open = true;
    const bool visible = ImGui::Begin(readLabel(L, 1), closable ? &open : nullptr, flags);
    lua::Stack::push(L, visible);
    lua::Stack::push(L, open);
    return 2;
}

int ImGuiLua::endWindow(lua_State* L) {
    (void)requireFrame(L);
    ImGui::End();
    return 0;
}

int ImGuiLua::text(lua_State* L) {
    (void)requireFrame(L);
    ImGui::TextUnformatted(readLabel(L, 1));
    return 0;
}

int ImGuiLua::textColored(lua_State* L) {
    (void)requireFrame(L);
    const math::Color color = lua::Stack::read<math::Color>(L, 1);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(color.r, color.g, color.b, color.a));
    ImGui::TextUnformatted(readLabel(L, 2));
    ImGui::PopStyleColor();
    return 0;
}

int ImGuiLua::textWrapped(lua_State* L) {
    (void)requireFrame(L);
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextUnformatted(readLabel(L, 1));
    ImGui::PopTextWrapPos();
    return 0;
}

int ImGuiLua::button(lua_State* L) {
    (void)requireFrame(L);
    lua::Stack::push(L, ImGui::Button(readLabel(L, 1), readOptionalSize(L, 2)));
    return 1;
}

int ImGuiLua::checkbox(lua_State* L) {
    (void)requireFrame(L);
    bool value = lua::Stack::read<bool>(L, 2);
    const bool changed = ImGui::Checkbox(readLabel(L, 1), &value);
    lua::Stack::push(L, value);
    return returnChanged(L, changed, 1);
}

// Dear ImGui hands the format to printf with the number alone, so it may convert that number once and escape other percent signs.
const char* ImGuiLua::readNumberFormat(lua_State* L, int index) {
    const std::string_view format = luaL_optstring(L, index, "%.3f");
    int conversions = 0;
    for (std::size_t position = format.find('%'); position != std::string_view::npos; position = format.find('%', position + 1)) {
        if (position + 1 < format.size() && format[position + 1] == '%') {
            ++position;
            continue;
        }
        position = format.find_first_not_of("-+ #0123456789.", position + 1);
        if (position != std::string_view::npos && format[position] == 'l') {
            ++position;
        }
        if (position >= format.size() || std::string_view("fFeEgGaA").find(format[position]) == std::string_view::npos || ++conversions > 1) {
            luaL_argerror(L, index, "the format may convert the number once, as in '%.1f', and writes a percent sign as '%%'");
        }
    }
    return format.data();
}

int ImGuiLua::sliderFloat(lua_State* L) {
    (void)requireFrame(L);
    auto value = lua::Stack::read<float>(L, 2);
    const bool changed = ImGui::SliderFloat(readLabel(L, 1), &value, lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4), readNumberFormat(L, 5));
    lua::Stack::push(L, value);
    return returnChanged(L, changed, 1);
}

int ImGuiLua::sliderInt(lua_State* L) {
    (void)requireFrame(L);
    auto value = lua::Stack::read<int>(L, 2);
    const bool changed = ImGui::SliderInt(readLabel(L, 1), &value, lua::Stack::read<int>(L, 3), lua::Stack::read<int>(L, 4));
    lua::Stack::push(L, value);
    return returnChanged(L, changed, 1);
}

int ImGuiLua::dragFloat(lua_State* L) {
    (void)requireFrame(L);
    auto value = lua::Stack::read<float>(L, 2);
    const bool changed = ImGui::DragFloat(readLabel(L, 1), &value, static_cast<float>(luaL_optnumber(L, 3, 1.0)), static_cast<float>(luaL_optnumber(L, 4, 0.0)), static_cast<float>(luaL_optnumber(L, 5, 0.0)));
    lua::Stack::push(L, value);
    return returnChanged(L, changed, 1);
}

int ImGuiLua::inputText(lua_State* L) {
    (void)requireFrame(L);
    std::string value = lua::Stack::read<std::string>(L, 2);
    const bool changed = lua_isnoneornil(L, 3) ? ImGui::InputText(readLabel(L, 1), &value) : ImGui::InputTextWithHint(readLabel(L, 1), readLabel(L, 3), &value);
    lua::Stack::push(L, value);
    return returnChanged(L, changed, 1);
}

int ImGuiLua::inputFloat(lua_State* L) {
    (void)requireFrame(L);
    auto value = lua::Stack::read<float>(L, 2);
    const bool changed = ImGui::InputFloat(readLabel(L, 1), &value, static_cast<float>(luaL_optnumber(L, 3, 0.0)));
    lua::Stack::push(L, value);
    return returnChanged(L, changed, 1);
}

int ImGuiLua::inputInt(lua_State* L) {
    (void)requireFrame(L);
    auto value = lua::Stack::read<int>(L, 2);
    const bool changed = ImGui::InputInt(readLabel(L, 1), &value, static_cast<int>(luaL_optinteger(L, 3, 1)));
    lua::Stack::push(L, value);
    return returnChanged(L, changed, 1);
}

int ImGuiLua::colorEdit(lua_State* L) {
    (void)requireFrame(L);
    const math::Color color = lua::Stack::read<math::Color>(L, 2);
    std::array<float, 4> channels{color.r, color.g, color.b, color.a};
    const bool changed = ImGui::ColorEdit4(readLabel(L, 1), channels.data());
    lua::Stack::push(L, math::Color{channels[0], channels[1], channels[2], channels[3]});
    return returnChanged(L, changed, 1);
}

// Shows a combo with combo(label, current, items), where current is a 1-based index, and returns whether it changed and the new index.
int ImGuiLua::combo(lua_State* L) {
    (void)requireFrame(L);
    auto current = lua::Stack::read<int>(L, 2) - 1;
    const std::vector<std::string> items = lua::Stack::read<std::vector<std::string>>(L, 3);
    std::string joined;
    for (const std::string& item : items) {
        joined += item;
        joined += '\0';
    }
    joined += '\0';
    const bool changed = ImGui::Combo(readLabel(L, 1), &current, joined.c_str());
    lua::Stack::push(L, current + 1);
    return returnChanged(L, changed, 1);
}

int ImGuiLua::selectable(lua_State* L) {
    (void)requireFrame(L);
    lua::Stack::push(L, ImGui::Selectable(readLabel(L, 1), lua_toboolean(L, 2) != 0));
    return 1;
}

int ImGuiLua::treeNode(lua_State* L) {
    (void)requireFrame(L);
    lua::Stack::push(L, ImGui::TreeNode(readLabel(L, 1)));
    return 1;
}

int ImGuiLua::treePop(lua_State* L) {
    (void)requireFrame(L);
    ImGui::TreePop();
    return 0;
}

int ImGuiLua::collapsingHeader(lua_State* L) {
    (void)requireFrame(L);
    lua::Stack::push(L, ImGui::CollapsingHeader(readLabel(L, 1)));
    return 1;
}

int ImGuiLua::separator(lua_State* L) {
    (void)requireFrame(L);
    ImGui::Separator();
    return 0;
}

int ImGuiLua::sameLine(lua_State* L) {
    (void)requireFrame(L);
    ImGui::SameLine(static_cast<float>(luaL_optnumber(L, 1, 0.0)), static_cast<float>(luaL_optnumber(L, 2, -1.0)));
    return 0;
}

int ImGuiLua::spacing(lua_State* L) {
    (void)requireFrame(L);
    ImGui::Spacing();
    return 0;
}

int ImGuiLua::dummy(lua_State* L) {
    (void)requireFrame(L);
    ImGui::Dummy({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)});
    return 0;
}

int ImGuiLua::beginChild(lua_State* L) {
    (void)requireFrame(L);
    lua::Stack::push(L, ImGui::BeginChild(readLabel(L, 1), readOptionalSize(L, 2), lua_toboolean(L, 4) != 0 ? ImGuiChildFlags_Borders : ImGuiChildFlags_None));
    return 1;
}

int ImGuiLua::endChild(lua_State* L) {
    (void)requireFrame(L);
    ImGui::EndChild();
    return 0;
}

int ImGuiLua::beginTabBar(lua_State* L) {
    (void)requireFrame(L);
    lua::Stack::push(L, ImGui::BeginTabBar(readLabel(L, 1)));
    return 1;
}

int ImGuiLua::endTabBar(lua_State* L) {
    (void)requireFrame(L);
    ImGui::EndTabBar();
    return 0;
}

int ImGuiLua::beginTabItem(lua_State* L) {
    (void)requireFrame(L);
    lua::Stack::push(L, ImGui::BeginTabItem(readLabel(L, 1)));
    return 1;
}

int ImGuiLua::endTabItem(lua_State* L) {
    (void)requireFrame(L);
    ImGui::EndTabItem();
    return 0;
}

int ImGuiLua::beginTable(lua_State* L) {
    (void)requireFrame(L);
    lua::Stack::push(L, ImGui::BeginTable(readLabel(L, 1), lua::Stack::read<int>(L, 2), ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg));
    return 1;
}

int ImGuiLua::endTable(lua_State* L) {
    (void)requireFrame(L);
    ImGui::EndTable();
    return 0;
}

int ImGuiLua::tableSetupColumn(lua_State* L) {
    (void)requireFrame(L);
    ImGui::TableSetupColumn(readLabel(L, 1));
    return 0;
}

int ImGuiLua::tableHeadersRow(lua_State* L) {
    (void)requireFrame(L);
    ImGui::TableHeadersRow();
    return 0;
}

int ImGuiLua::tableNextRow(lua_State* L) {
    (void)requireFrame(L);
    ImGui::TableNextRow();
    return 0;
}

int ImGuiLua::tableNextColumn(lua_State* L) {
    (void)requireFrame(L);
    lua::Stack::push(L, ImGui::TableNextColumn());
    return 1;
}

int ImGuiLua::progressBar(lua_State* L) {
    (void)requireFrame(L);
    ImGui::ProgressBar(lua::Stack::read<float>(L, 1), {static_cast<float>(luaL_optnumber(L, 2, -1.0)), static_cast<float>(luaL_optnumber(L, 3, 0.0))}, luaL_optstring(L, 4, nullptr));
    return 0;
}

// Plots numbers with plotLines(label, values[, overlay, minimum, maximum, width, height]).
int ImGuiLua::plot(lua_State* L, bool histogram) {
    (void)requireFrame(L);
    const std::vector<float> values = lua::Stack::read<std::vector<float>>(L, 2);
    const char* overlay = luaL_optstring(L, 3, nullptr);
    const auto minimum = static_cast<float>(luaL_optnumber(L, 4, FLT_MAX));
    const auto maximum = static_cast<float>(luaL_optnumber(L, 5, FLT_MAX));
    const ImVec2 size = readOptionalSize(L, 6);
    if (histogram) {
        ImGui::PlotHistogram(readLabel(L, 1), values.data(), static_cast<int>(values.size()), 0, overlay, minimum, maximum, size);
    } else {
        ImGui::PlotLines(readLabel(L, 1), values.data(), static_cast<int>(values.size()), 0, overlay, minimum, maximum, size);
    }
    return 0;
}

int ImGuiLua::plotLines(lua_State* L) {
    return plot(L, false);
}

int ImGuiLua::plotHistogram(lua_State* L) {
    return plot(L, true);
}

int ImGuiLua::image(lua_State* L) {
    Backend& backend = requireFrame(L);
    const graphics::Texture texture = lua::Stack::read<graphics::Texture>(L, 1);
    ImGui::Image(backend.getTextureReference(texture), {static_cast<float>(luaL_optnumber(L, 2, texture.getSize().x)), static_cast<float>(luaL_optnumber(L, 3, texture.getSize().y))});
    return 0;
}

int ImGuiLua::setNextWindowPos(lua_State* L) {
    (void)requireFrame(L);
    ImGui::SetNextWindowPos({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)}, lua_toboolean(L, 3) != 0 ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
    return 0;
}

int ImGuiLua::setNextWindowSize(lua_State* L) {
    (void)requireFrame(L);
    ImGui::SetNextWindowSize({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)}, lua_toboolean(L, 3) != 0 ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
    return 0;
}

int ImGuiLua::setNextItemWidth(lua_State* L) {
    (void)requireFrame(L);
    ImGui::SetNextItemWidth(lua::Stack::read<float>(L, 1));
    return 0;
}

int ImGuiLua::isItemHovered(lua_State* L) {
    (void)requireFrame(L);
    lua::Stack::push(L, ImGui::IsItemHovered());
    return 1;
}

int ImGuiLua::setTooltip(lua_State* L) {
    (void)requireFrame(L);
    ImGui::SetTooltip("%s", readLabel(L, 1));
    return 0;
}

int ImGuiLua::openPopup(lua_State* L) {
    (void)requireFrame(L);
    ImGui::OpenPopup(readLabel(L, 1));
    return 0;
}

int ImGuiLua::beginPopup(lua_State* L) {
    (void)requireFrame(L);
    lua::Stack::push(L, ImGui::BeginPopup(readLabel(L, 1)));
    return 1;
}

int ImGuiLua::endPopup(lua_State* L) {
    (void)requireFrame(L);
    ImGui::EndPopup();
    return 0;
}

int ImGuiLua::closeCurrentPopup(lua_State* L) {
    (void)requireFrame(L);
    ImGui::CloseCurrentPopup();
    return 0;
}

int ImGuiLua::pushId(lua_State* L) {
    (void)requireFrame(L);
    ImGui::PushID(readLabel(L, 1));
    return 0;
}

int ImGuiLua::popId(lua_State* L) {
    (void)requireFrame(L);
    ImGui::PopID();
    return 0;
}

// Uses a font the UI knows for the next widgets with pushFont(name, size).
int ImGuiLua::pushFont(lua_State* L) {
    Backend& backend = requireFrame(L);
    ImGui::PushFont(backend.getFont(lua::Stack::read<std::string_view>(L, 1)), lua::Stack::read<float>(L, 2));
    return 0;
}

int ImGuiLua::popFont(lua_State* L) {
    (void)requireFrame(L);
    ImGui::PopFont();
    return 0;
}

int ImGuiLua::showDemoWindow(lua_State* L) {
    (void)requireFrame(L);
    ImGui::ShowDemoWindow();
    return 0;
}

int ImGuiLua::showMetricsWindow(lua_State* L) {
    (void)requireFrame(L);
    ImGui::ShowMetricsWindow();
    return 0;
}

int ImGuiLua::framerate(lua_State* L) {
    (void)requireFrame(L);
    lua::Stack::push(L, ImGui::GetIO().Framerate);
    return 1;
}

int ImGuiLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"beginWindow", &lua::Binding::native<&beginWindow>}, {"endWindow", &lua::Binding::native<&endWindow>}, {"text", &lua::Binding::native<&text>}, {"textColored", &lua::Binding::native<&textColored>}, {"textWrapped", &lua::Binding::native<&textWrapped>}, {"button", &lua::Binding::native<&button>}, {"checkbox", &lua::Binding::native<&checkbox>}, {"sliderFloat", &lua::Binding::native<&sliderFloat>}, {"sliderInt", &lua::Binding::native<&sliderInt>}, {"dragFloat", &lua::Binding::native<&dragFloat>}, {"inputText", &lua::Binding::native<&inputText>}, {"inputFloat", &lua::Binding::native<&inputFloat>}, {"inputInt", &lua::Binding::native<&inputInt>}, {"colorEdit", &lua::Binding::native<&colorEdit>}, {"combo", &lua::Binding::native<&combo>}, {"selectable", &lua::Binding::native<&selectable>}, {"treeNode", &lua::Binding::native<&treeNode>}, {"treePop", &lua::Binding::native<&treePop>}, {"collapsingHeader", &lua::Binding::native<&collapsingHeader>}, {"separator", &lua::Binding::native<&separator>}, {"sameLine", &lua::Binding::native<&sameLine>}, {"spacing", &lua::Binding::native<&spacing>}, {"dummy", &lua::Binding::native<&dummy>}, {"beginChild", &lua::Binding::native<&beginChild>}, {"endChild", &lua::Binding::native<&endChild>}, {"beginTabBar", &lua::Binding::native<&beginTabBar>}, {"endTabBar", &lua::Binding::native<&endTabBar>}, {"beginTabItem", &lua::Binding::native<&beginTabItem>}, {"endTabItem", &lua::Binding::native<&endTabItem>}, {"beginTable", &lua::Binding::native<&beginTable>}, {"endTable", &lua::Binding::native<&endTable>}, {"tableSetupColumn", &lua::Binding::native<&tableSetupColumn>}, {"tableHeadersRow", &lua::Binding::native<&tableHeadersRow>}, {"tableNextRow", &lua::Binding::native<&tableNextRow>}, {"tableNextColumn", &lua::Binding::native<&tableNextColumn>}, {"progressBar", &lua::Binding::native<&progressBar>}, {"plotLines", &lua::Binding::native<&plotLines>}, {"plotHistogram", &lua::Binding::native<&plotHistogram>}, {"image", &lua::Binding::native<&image>}, {"setNextWindowPos", &lua::Binding::native<&setNextWindowPos>}, {"setNextWindowSize", &lua::Binding::native<&setNextWindowSize>}, {"setNextItemWidth", &lua::Binding::native<&setNextItemWidth>}, {"isItemHovered", &lua::Binding::native<&isItemHovered>}, {"setTooltip", &lua::Binding::native<&setTooltip>}, {"openPopup", &lua::Binding::native<&openPopup>}, {"beginPopup", &lua::Binding::native<&beginPopup>}, {"endPopup", &lua::Binding::native<&endPopup>}, {"closeCurrentPopup", &lua::Binding::native<&closeCurrentPopup>}, {"pushId", &lua::Binding::native<&pushId>}, {"popId", &lua::Binding::native<&popId>}, {"pushFont", &lua::Binding::native<&pushFont>}, {"popFont", &lua::Binding::native<&popFont>}, {"showDemoWindow", &lua::Binding::native<&showDemoWindow>}, {"showMetricsWindow", &lua::Binding::native<&showMetricsWindow>}, {"framerate", &lua::Binding::native<&framerate>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void ImGuiLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.imgui", &open);
}

} // namespace haylen::ui
