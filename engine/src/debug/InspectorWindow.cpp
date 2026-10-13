#include "debug/InspectorWindow.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <exception>

namespace haylen::debug {

bool InspectorWindow::draw(lua_State* L, double now) {
    bool open = true;
    if (!ImGui::Begin("Inspector", &open)) {
        ImGui::End();
        return open;
    }
    if (!failure.empty()) {
        ImGui::TextColored({1.0F, 0.45F, 0.4F, 1.0F}, "%s", failure.c_str());
    }
    if (expired) {
        for (auto& kept : snapshots) {
            kept.second.time = -1.0;
        }
        expired = false;
    }
    std::vector<std::string> path;
    drawNode(L, path, now);
    ImGui::End();
    if (!open) {
        snapshots.clear();
        edits.clear();
        failure.clear();
        activeField.clear();
    }
    return open;
}

void InspectorWindow::drawNode(lua_State* L, std::vector<std::string>& path, double now) {
    const Snapshot& snapshot = snapshotOf(L, path, now);
    if (!snapshot.error.empty()) {
        ImGui::TextDisabled("%s", snapshot.error.c_str());
        return;
    }
    for (const Inspector::Entry& entry : snapshot.entries) {
        path.push_back(entry.key);
        ImGui::PushID(entry.key.c_str());
        if (entry.expandable) {
            if (ImGui::TreeNode("##node", "%s  %s %s", entry.key.c_str(), entry.type.c_str(), entry.value.c_str())) {
                drawNode(L, path, now);
                ImGui::TreePop();
            }
        } else {
            drawField(L, path, entry);
        }
        ImGui::PopID();
        path.pop_back();
    }
}

// A field shows the value it read until the player edits it, and Enter sends the edit through the setter of the value.
void InspectorWindow::drawField(lua_State* L, const std::vector<std::string>& path, const Inspector::Entry& entry) {
    ImGui::Bullet();
    ImGui::SameLine();
    ImGui::TextUnformatted(entry.key.c_str());
    ImGui::SameLine();
    if (!entry.editable) {
        ImGui::TextDisabled("%s %s", entry.type.c_str(), entry.value.c_str());
        return;
    }
    const std::string key = joinPath(path);
    std::string& text = edits[key];
    if (activeField != key) {
        text = entry.value;
    }
    ImGui::SetNextItemWidth(220.0F);
    const bool entered = ImGui::InputText("##value", &text, ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::IsItemActive()) {
        activeField = key;
    } else if (activeField == key) {
        activeField.clear();
    }
    if (entered) {
        try {
            Inspector::set(L, path, text);
            failure.clear();
        } catch (const std::exception& error) {
            failure = error.what();
        }
        expired = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", entry.type.c_str());
}

const InspectorWindow::Snapshot& InspectorWindow::snapshotOf(lua_State* L, const std::vector<std::string>& path, double now) {
    Snapshot& snapshot = snapshots[joinPath(path)];
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

std::string InspectorWindow::joinPath(const std::vector<std::string>& path) {
    std::string joined;
    for (const std::string& key : path) {
        joined += joined.empty() ? key : "." + key;
    }
    return joined;
}

} // namespace haylen::debug
