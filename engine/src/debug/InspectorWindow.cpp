#include "debug/InspectorWindow.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <format>
#include <span>
#include <utility>

#include "debug/DebugWindow.hpp"
#include "haylen/ui/Backend.hpp"

namespace haylen::debug {

InspectorWindow::InspectorWindow(Clock source) : clock(source ? std::move(source) : Clock(&getSteadySeconds)) {}

double InspectorWindow::getSteadySeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool InspectorWindow::draw(Inspector::Provider& provider, ui::Backend& backend) {
    const math::Rect safe = backend.getSafeRect();
    const float width = std::min(kWidth, safe.width - DebugWindow::kMargin * 2.0F);
    const math::Rect initial{safe.x + safe.width - width - DebugWindow::kMargin, safe.y + DebugWindow::kMargin, width, safe.height - DebugWindow::kMargin * 2.0F};
    bool open = true;
    if (DebugWindow::begin("Inspector", &open, backend, initial)) {
        ImGui::PushStyleColor(ImGuiCol_Text, failed ? DebugWindow::kErrorColor : DebugWindow::kMutedColor);
        ImGui::TextWrapped("%s", status.empty() ? kHint.data() : status.c_str());
        ImGui::PopStyleColor();

        if (std::exchange(expired, false)) {
            for (auto& [key, snapshot] : snapshots) {
                snapshot.time = -1.0;
            }
        }

        constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("##values", 2, flags)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.4F);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.0F);
            drawRows(provider, 0, rootPath, clock());
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
    status.clear();
    failed = false;
    activeField.clear();
}

// Each value is a row whose name opens the values it holds in the rows below it, one level deeper. The map keeps every snapshot where it is while rows below add theirs.
void InspectorWindow::drawRows(Inspector::Provider& provider, std::size_t depth, const std::string& key, double now) {
    const Snapshot& snapshot = snapshotOf(provider, depth, key, now);
    if (!snapshot.error.empty()) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", snapshot.error.c_str());
        return;
    }

    const std::vector<Inspector::Entry>& entries = snapshot.listing.entries;
    if (entries.empty()) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", "Empty");
        return;
    }

    if (path.size() == depth) {
        path.emplace_back();
    }

    for (std::size_t index = 0; index < entries.size(); ++index) {
        const Inspector::Entry& entry = entries[index];
        const std::string& entryPath = snapshot.paths[index];
        path[depth] = entry.key;
        ImGui::PushID(entry.key.c_str());
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        if (depth == 0 && entry.key == kOpenRoot) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        }

        const ImGuiTreeNodeFlags kind = entry.expandable ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        const bool opened = ImGui::TreeNodeEx("##row", kind | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", entry.key.c_str());
        ImGui::TableNextColumn();
        drawValue(provider, depth + 1, entryPath, entry);

        if (opened && entry.expandable) {
            drawRows(provider, depth + 1, entryPath, now);
            ImGui::TreePop();
        }

        ImGui::PopID();
    }
}

// A boolean shows a check box and a number or a string a field, while every other value shows what it holds, or its type when it tells nothing more, such as an object of a bound type.
void InspectorWindow::drawValue(Inspector::Provider& provider, std::size_t depth, const std::string& key, const Inspector::Entry& entry) {
    if (!entry.editable) {
        ImGui::TextDisabled("%s", entry.value.empty() ? entry.type.c_str() : entry.value.c_str());
        return;
    }

    if (entry.type == "boolean") {
        bool checked = entry.value == "true";
        if (ImGui::Checkbox("##value", &checked)) {
            edit(provider, depth, checked ? "true" : "false");
        }

        return;
    }

    drawField(provider, depth, key, entry);
}

// A field shows the value it read until the player edits it, and the edit goes through the setter of the value once Enter confirms it or the field lets go of the focus.
void InspectorWindow::drawField(Inspector::Provider& provider, std::size_t depth, const std::string& key, const Inspector::Entry& entry) {
    auto found = edits.find(key);
    if (found == edits.end()) {
        found = edits.emplace(key, entry.value).first;
    }

    std::string& text = found->second;
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
        edit(provider, depth, text);
    }
}

void InspectorWindow::edit(Inspector::Provider& provider, std::size_t depth, std::string_view text) {
    const std::span<const std::string> edited(path.data(), depth);
    try {
        provider.set(edited, text);
        status = std::format("Set \"{}\" to {}.", Inspector::joinPath(edited), text);
        failed = false;
    } catch (const std::exception& error) {
        status = error.what();
        failed = true;
    }

    expired = true;
}

// A refresh lists the values again into the storage of the snapshot, and the path of each value takes the storage it had.
const InspectorWindow::Snapshot& InspectorWindow::snapshotOf(Inspector::Provider& provider, std::size_t depth, const std::string& key, double now) {
    auto found = snapshots.find(key);
    if (found == snapshots.end()) {
        found = snapshots.emplace(key, Snapshot{}).first;
    }

    Snapshot& snapshot = found->second;
    if (snapshot.time >= 0.0 && now - snapshot.time < kRefreshSeconds) {
        return snapshot;
    }

    snapshot.time = now;
    try {
        provider.list(std::span<const std::string>(path.data(), depth), snapshot.listing);
        snapshot.error.clear();
    } catch (const std::exception& error) {
        snapshot.listing.entries.clear();
        snapshot.error = error.what();
    }

    const std::vector<Inspector::Entry>& entries = snapshot.listing.entries;
    snapshot.paths.resize(entries.size());
    for (std::size_t index = 0; index < entries.size(); ++index) {
        snapshot.paths[index] = key;
        Inspector::appendKey(snapshot.paths[index], entries[index].key);
    }

    return snapshot;
}

} // namespace haylen::debug
