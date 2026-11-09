#include "debug/InspectorWindow.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <exception>
#include <utility>

#include "debug/DebugWindow.hpp"
#include "haylen/ui/Backend.hpp"

namespace haylen::debug {

bool InspectorWindow::draw(lua_State* L, ui::Backend& backend, double now) {
    const math::Rect safe = backend.getSafeRect();
    const float width = std::min(kWidth, safe.width - DebugWindow::kMargin * 2.0F);
    const math::Rect initial{safe.x + safe.width - width - DebugWindow::kMargin, safe.y + DebugWindow::kMargin, width, safe.height - DebugWindow::kMargin * 2.0F};
    bool open = true;
    if (DebugWindow::begin("Inspector", &open, backend, initial)) {
        if (!failure.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, DebugWindow::kErrorColor);
            ImGui::TextWrapped("%s", failure.c_str());
            ImGui::PopStyleColor();
        }
        if (std::exchange(expired, false)) {
            for (auto& [key, snapshot] : snapshots) {
                snapshot.time = -1.0;
            }
        }

        constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("##values", 2, flags)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.4F);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.0F);
            std::vector<std::string> path;
            drawRows(L, path, now);
            ImGui::EndTable();
        }
    }
    DebugWindow::end();

    if (!open) {
        reset();
    }
    return open;
}

void InspectorWindow::reset() {
    snapshots.clear();
    edits.clear();
    failure.clear();
    activeField.clear();
}

// Each value is a row whose name opens the values it holds in the rows below it, one level deeper.
void InspectorWindow::drawRows(lua_State* L, std::vector<std::string>& path, double now) {
    const Snapshot& snapshot = snapshotOf(L, path, now);
    if (!snapshot.error.empty()) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", snapshot.error.c_str());
        return;
    }
    if (snapshot.entries.empty()) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", "Empty");
        return;
    }

    for (const Inspector::Entry& entry : snapshot.entries) {
        path.push_back(entry.key);
        ImGui::PushID(entry.key.c_str());
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (path.size() == 1 && entry.key == kOpenRoot) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        }
        const ImGuiTreeNodeFlags kind = entry.expandable ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        const bool opened = ImGui::TreeNodeEx("##row", kind | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", entry.key.c_str());
        ImGui::TableNextColumn();
        drawValue(L, path, entry);

        if (opened && entry.expandable) {
            drawRows(L, path, now);
            ImGui::TreePop();
        }
        ImGui::PopID();
        path.pop_back();
    }
}

// A boolean shows a check box and a number or a string a field, while every other value shows what it holds, or its type when it tells nothing more, such as an object of a bound type.
void InspectorWindow::drawValue(lua_State* L, const std::vector<std::string>& path, const Inspector::Entry& entry) {
    if (!entry.editable) {
        ImGui::TextDisabled("%s", entry.value.empty() ? entry.type.c_str() : entry.value.c_str());
        return;
    }
    if (entry.type == "boolean") {
        bool checked = entry.value == "true";
        if (ImGui::Checkbox("##value", &checked)) {
            edit(L, path, checked ? "true" : "false");
        }
        return;
    }
    drawField(L, path, entry);
}

// A field shows the value it read until the player edits it, and the edit goes through the setter of the value once Enter confirms it or the field lets go of the focus.
void InspectorWindow::drawField(lua_State* L, const std::vector<std::string>& path, const Inspector::Entry& entry) {
    const std::string key = Inspector::joinPath(path);
    std::string& text = edits[key];
    if (activeField != key) {
        text = entry.value;
    }
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputText("##value", &text);
    if (ImGui::IsItemActive()) {
        activeField = key;
    } else if (activeField == key) {
        activeField.clear();
    }

    if (ImGui::IsItemDeactivatedAfterEdit()) {
        edit(L, path, text);
    }
}

void InspectorWindow::edit(lua_State* L, const std::vector<std::string>& path, std::string_view text) {
    try {
        Inspector::set(L, path, text);
        failure.clear();
    } catch (const std::exception& error) {
        failure = error.what();
    }
    expired = true;
}

const InspectorWindow::Snapshot& InspectorWindow::snapshotOf(lua_State* L, const std::vector<std::string>& path, double now) {
    Snapshot& snapshot = snapshots[Inspector::joinPath(path)];
    if (snapshot.time >= 0.0 && now - snapshot.time < kRefreshSeconds) {
        return snapshot;
    }
    snapshot.time = now;
    try {
        snapshot.entries = Inspector::list(L, path);
        snapshot.error.clear();
    } catch (const std::exception& error) {
        snapshot.entries.clear();
        snapshot.error = error.what();
    }
    return snapshot;
}

} // namespace haylen::debug
