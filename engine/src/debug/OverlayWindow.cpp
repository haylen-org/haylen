#include "debug/OverlayWindow.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <format>
#include <iterator>
#include <string>

#include "debug/DebugWindow.hpp"
#include "haylen/content/ContentManager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/ProcessMode.hpp"
#include "haylen/core/Scene.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Backend.hpp"

namespace haylen::debug {

ImVec4 OverlayWindow::getLevelColor(core::Log::Level level) noexcept {
    switch (level) {
    case core::Log::Level::Debug:
        return DebugWindow::kMutedColor;
    case core::Log::Level::Warning:
        return DebugWindow::kWarningColor;
    case core::Log::Level::Error:
        return DebugWindow::kErrorColor;
    case core::Log::Level::Info:
        break;
    }

    return DebugWindow::kTextColor;
}

// Writes the text into the buffer, which holds the longest count of bytes, and returns it.
const char* OverlayWindow::formatBytes(ByteText& text, std::int64_t bytes) noexcept {
    const auto value = static_cast<double>(bytes);
    const auto limit = static_cast<std::ptrdiff_t>(text.size() - 1);
    std::format_to_n_result<char*> written{};
    if (value >= 1024.0 * 1024.0) {
        written = std::format_to_n(text.data(), limit, "{:.1f} MB", value / (1024.0 * 1024.0));
    } else if (value >= 1024.0) {
        written = std::format_to_n(text.data(), limit, "{:.1f} KB", value / 1024.0);
    } else {
        written = std::format_to_n(text.data(), limit, "{} B", bytes);
    }

    *written.out = '\0';
    return text.data();
}

void OverlayWindow::drawFrame(const Stats::Frame& frame) {
    ImGui::TextWrapped("%.0f FPS, %.2f ms average, %.2f ms last frame", frame.fps, frame.average, frame.milliseconds);
    ImGui::TextWrapped("Fastest %.2f ms, slowest %.2f ms, 1%% low %.2f ms, %u fixed steps", frame.minimum, frame.maximum, frame.onePercentLow, frame.fixedSteps);
    ImGui::PlotLines("##frames", history.data(), static_cast<int>(history.size()), 0, nullptr, 0.0F, 50.0F, {-1.0F, 80.0F});
}

void OverlayWindow::drawEventLoop(const Stats::EventLoop& loop) {
    const char* activity = loop.ran ? "work ran" : "nothing ran";
    if (!loop.pending) {
        ImGui::TextWrapped("Event loop budget %.2f ms, %s, nothing pending", loop.budget, activity);
    } else if (loop.idle == 0) {
        ImGui::TextWrapped("Event loop budget %.2f ms, %s, work left for the next frame", loop.budget, activity);
    } else if (loop.idle > 0) {
        ImGui::TextWrapped("Event loop budget %.2f ms, %s, next timer in %lld ms", loop.budget, activity, loop.idle);
    } else {
        ImGui::TextWrapped("Event loop budget %.2f ms, %s, waiting for sockets and workers", loop.budget, activity);
    }
}

void OverlayWindow::drawProfiler(const Profiler& profiler) {
    if (!ImGui::BeginTable("##scopes", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        return;
    }

    ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch, 3.0F);
    ImGui::TableSetupColumn("Time in ms", ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableHeadersRow();

    for (const ProfileSample& sample : profiler.getLastFrame()) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Indent(static_cast<float>(sample.depth) * 16.0F + 1.0F);
        ImGui::TextUnformatted(sample.name.c_str());
        ImGui::Unindent(static_cast<float>(sample.depth) * 16.0F + 1.0F);
        ImGui::TableNextColumn();
        ImGui::Text("%.2f", sample.milliseconds);
        ImGui::TableNextColumn();
        ImGui::Text("%u", sample.calls);
    }

    ImGui::EndTable();
}

void OverlayWindow::drawRendering(const graphics2d::Renderer::Stats& rendering) {
    ImGui::TextWrapped("Draw calls %zu, passes %zu, canvases %zu", rendering.drawCalls, rendering.passes, rendering.canvases);
    ImGui::TextWrapped("Sprites %zu, instances %zu, lights %zu, occluders %zu, shadows %zu", rendering.sprites, rendering.instances, rendering.lights, rendering.occluders, rendering.shadows);
    ImGui::TextWrapped("Vertices %zu, indices %zu, texture switches %zu", rendering.vertices, rendering.indices, rendering.textureSwitches);
    ByteText uploaded{};
    ImGui::TextWrapped("Uploaded %s", formatBytes(uploaded, static_cast<std::int64_t>(rendering.uploadedBytes)));
}

void OverlayWindow::drawResources() {
    std::array<ByteText, 7> bytes{};
    ImGui::TextWrapped("Lua %s, textures %s, render targets %s, static batches %s, render buffers %s, sounds %s, GPU memory held for later %s", formatBytes(bytes[0], static_cast<std::int64_t>(stats.memory.lua)), formatBytes(bytes[1], stats.memory.textures), formatBytes(bytes[2], stats.memory.targets), formatBytes(bytes[3], stats.memory.batches), formatBytes(bytes[4], stats.memory.buffers), formatBytes(bytes[5], stats.memory.sounds), formatBytes(bytes[6], static_cast<std::int64_t>(stats.memory.gpuSpare)));
    const Stats::Counts& counts = stats.counts;
    ImGui::TextWrapped("Scenes %zu, tweens %zu, timers %zu, voices %zu, sockets %zu", counts.scenes, counts.tweens, counts.timers, counts.voices, counts.sockets);
    ImGui::TextWrapped("Assets cached %zu, loading %zu", counts.assetsCached, counts.assetsPending);

    // The text of the buses keeps its storage, so it formats in place.
    voices = "Voices by bus:";
    for (const audio::Mixer::BusStats& bus : stats.buses) {
        std::format_to(std::back_inserter(voices), " {} {}{}", bus.name, bus.voices, bus.processing ? "" : " (paused)");
    }

    ImGui::TextWrapped("%s", voices.c_str());

    ImGui::TextWrapped("Bodies %llu, contacts %llu, particles %llu", static_cast<unsigned long long>(counts.bodies), static_cast<unsigned long long>(counts.contacts), static_cast<unsigned long long>(counts.particles));

    // A pool that fills up makes the next GPU object fail, so the bar turns red past nine tenths.
    for (const graphics::Device::Pool& pool : stats.pools) {
        const float fraction = pool.size > 0 ? static_cast<float>(pool.used) / static_cast<float>(pool.size) : 0.0F;
        Label label{};
        const auto written = std::format_to_n(label.data(), label.size() - 1, "Pool \"{}\" {} / {}", pool.name, pool.used, pool.size);
        *written.out = '\0';
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, fraction > 0.9F ? DebugWindow::kErrorColor : DebugWindow::kAccentColor);
        ImGui::ProgressBar(fraction, {-1.0F, 0.0F}, label.data());
        ImGui::PopStyleColor();
    }
}

void OverlayWindow::drawContent(const content::ContentManager& content) {
    const content::ContentManager::Statistics statistics = content.getStatistics();
    ImGui::TextWrapped("Generation %llu, %llu files, %s", static_cast<unsigned long long>(statistics.generation), static_cast<unsigned long long>(statistics.catalogFiles), content.isProtected() ? "protected release" : "folder of the app");
    std::array<ByteText, 4> bytes{};
    ImGui::TextWrapped("Shards open %llu, reads running %llu, chunks decoded %llu (%s in %.1f ms)", static_cast<unsigned long long>(statistics.openShards), static_cast<unsigned long long>(statistics.activeReads), static_cast<unsigned long long>(statistics.decodedChunks), formatBytes(bytes[0], static_cast<std::int64_t>(statistics.decodedBytes)), statistics.decodeMilliseconds);
    ImGui::TextWrapped("Shards installed %llu, from the store or the server %llu, chunks missing %llu, of them prefetched %llu", static_cast<unsigned long long>(statistics.installedShards), static_cast<unsigned long long>(statistics.remoteShards), static_cast<unsigned long long>(statistics.missingChunks), static_cast<unsigned long long>(statistics.pendingPrefetch));
    ImGui::TextWrapped("Store %llu records (%s)", static_cast<unsigned long long>(statistics.storedRecords), formatBytes(bytes[1], static_cast<std::int64_t>(statistics.storedBytes)));
    ImGui::TextWrapped("Downloaded %s in %llu requests, %s/s, %llu records waiting, %llu transfers, %llu resumed", formatBytes(bytes[2], static_cast<std::int64_t>(statistics.downloadedBytes)), static_cast<unsigned long long>(statistics.requests), formatBytes(bytes[3], static_cast<std::int64_t>(statistics.downloadRate)), static_cast<unsigned long long>(statistics.wantedRecords), static_cast<unsigned long long>(statistics.activeTransfers), static_cast<unsigned long long>(statistics.resumedRecords));

    if (statistics.reusedBytes > 0) {
        ImGui::TextWrapped("The offered update reuses %s that the device holds", formatBytes(bytes[0], static_cast<std::int64_t>(statistics.reusedBytes)));
    }
}

void OverlayWindow::drawObjects(const std::vector<ObjectCounter::Snapshot>& objects) {
    if (!ImGui::BeginTable("##objects", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        return;
    }

    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 3.0F);
    ImGui::TableSetupColumn("Alive", ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableSetupColumn("Created", ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableSetupColumn("Destroyed", ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableSetupColumn("Memory", ImGuiTableColumnFlags_WidthStretch, 1.2F);
    ImGui::TableHeadersRow();

    for (const ObjectCounter::Snapshot& object : objects) {
        if (object.created == 0) {
            continue;
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(object.name.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%llu", static_cast<unsigned long long>(object.alive));
        ImGui::TableNextColumn();
        ImGui::Text("%llu", static_cast<unsigned long long>(object.created));
        ImGui::TableNextColumn();
        ImGui::Text("%llu", static_cast<unsigned long long>(object.destroyed));
        ImGui::TableNextColumn();

        if (object.bytes > 0) {
            ByteText bytes{};
            ImGui::TextUnformatted(formatBytes(bytes, object.bytes));
        }
    }

    ImGui::EndTable();
}

// The scenes from the bottom of the stack up, each with the state it is in and the process mode it resolves to, and whether the scenes below it keep rendering.
void OverlayWindow::drawScenes(const core::SceneManager& scenes) {
    if (scenes.empty()) {
        ImGui::TextUnformatted("No scene is on the stack.");
        return;
    }

    if (!ImGui::BeginTable("##scenes", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        return;
    }

    ImGui::TableSetupColumn("Scene", ImGuiTableColumnFlags_WidthStretch, 3.0F);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch, 1.2F);
    ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthStretch, 1.2F);
    ImGui::TableSetupColumn("Transparent", ImGuiTableColumnFlags_WidthStretch, 1.6F);
    ImGui::TableHeadersRow();

    for (std::size_t index = 0; index < scenes.size(); ++index) {
        const core::Scene& scene = scenes.at(index);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%zu. %s", index + 1, scene.getName().c_str());
        ImGui::TableNextColumn();
        const std::string_view state = core::Scene::stateName(scene.getState());
        ImGui::TextUnformatted(state.data(), state.data() + state.size());
        ImGui::TableNextColumn();
        const std::string_view mode = core::EnumNames<core::ProcessMode>::name(scenes.getProcessMode(index));
        ImGui::TextUnformatted(mode.data(), mode.data() + mode.size());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(scene.isTransparent() ? "Yes" : "No");
    }

    ImGui::EndTable();
}

// A checkbox for every drawing the engine and the app know, which switches it like the API does. The checkboxes take their ids inside the section, since the app names its drawings and a name may match another item of the window.
void OverlayWindow::drawDrawings(plugins::DebugPlugin& plugin) {
    ImGui::PushStyleColor(ImGuiCol_Text, DebugWindow::kMutedColor);
    ImGui::TextWrapped("%s", kDrawingsHint.data());
    ImGui::PopStyleColor();

    ImGui::PushID("drawings");
    plugin.readDrawingNames(drawingNames);
    for (const std::string& name : drawingNames) {
        bool enabled = plugin.isDrawing(name);
        if (ImGui::Checkbox(name.c_str(), &enabled)) {
            plugin.setDrawing(name, enabled);
        }

        ImGui::SameLine();
    }

    ImGui::NewLine();
    ImGui::PopID();
}

std::size_t OverlayWindow::countStale(const std::vector<core::EventBus::Topic>& topics) noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(topics, [](const core::EventBus::Topic& topic) { return topic.stale > 0; }));
}

void OverlayWindow::drawTopics(const char* id, const std::vector<core::EventBus::Topic>& topics) {
    if (topics.empty() || !ImGui::BeginTable(id, 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        return;
    }

    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 3.0F);
    ImGui::TableSetupColumn("Listeners", ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableSetupColumn("Emits", ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableSetupColumn("Owner gone", ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableHeadersRow();

    for (const core::EventBus::Topic& topic : topics) {
        ImGui::TableNextRow();

        if (topic.stale > 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, getLevelColor(core::Log::Level::Warning));
        }

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(topic.name.empty() ? "(Unnamed)" : topic.name.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%zu", topic.listeners);
        ImGui::TableNextColumn();
        ImGui::Text("%llu", static_cast<unsigned long long>(topic.emissions));
        ImGui::TableNextColumn();
        ImGui::Text("%zu", topic.stale);

        if (topic.stale > 0) {
            ImGui::PopStyleColor();
        }
    }

    ImGui::EndTable();
}

// Each plot takes its id from its monitor inside the section, and the history of each monitor in turn reads into the one vector the window keeps.
void OverlayWindow::drawMonitors() {
    for (const std::shared_ptr<Monitor>& monitor : monitors) {
        monitor->readHistory(history);
        if (history.empty()) {
            continue;
        }

        const auto [lowest, highest] = std::ranges::minmax(history);
        Label label{};
        const auto written = std::format_to_n(label.data(), label.size() - 1, "{} {:.3g}", monitor->getName(), monitor->getValue());
        *written.out = '\0';
        ImGui::PushID(monitor.get());
        ImGui::PlotLines("##monitor", history.data(), static_cast<int>(history.size()), 0, label.data(), lowest, std::max(highest, lowest + 1.0F), {-1.0F, 48.0F});
        ImGui::PopID();
    }
}

void OverlayWindow::drawLog() {
    if (ImGui::BeginChild("##log", {0.0F, 220.0F}, ImGuiChildFlags_Borders)) {
        for (const LogLine& line : log) {
            ImGui::PushStyleColor(ImGuiCol_Text, getLevelColor(line.level));
            ImGui::TextWrapped("%s", line.text.c_str());
            ImGui::PopStyleColor();
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0F);
        }
    }

    ImGui::EndChild();
}

bool OverlayWindow::draw(core::Engine& engine, ui::Backend& backend, plugins::DebugPlugin& plugin) {
    const math::Rect safe = backend.getSafeRect();
    const math::Rect initial{safe.x + DebugWindow::kMargin, safe.y + DebugWindow::kMargin, std::min(kWidth, safe.width - DebugWindow::kMargin * 2.0F), std::min(kHeight, safe.height - DebugWindow::kMargin * 2.0F)};
    bool open = true;
    if (!DebugWindow::begin("Statistics", &open, backend, engine.getPlugin<plugins::UiPlugin>().getContext(), initial)) {
        DebugWindow::end();
        return open;
    }

    plugin.updateStats(engine, stats);
    engine.getProfiler().readFrameHistory(history);
    plugin.readMonitors(monitors);
    plugin.readRecentLog(log);

    // Listeners whose owner died stay connected until their topic emits again, so the warning shows even while their section is closed.
    const Profiler& profiler = engine.getProfiler();
    engine.getEvents().readTopics(events);
    plugin.readScriptSignals(signals);
    const std::size_t stale = countStale(events) + countStale(signals);
    drawFrame(stats.frame);
    drawEventLoop(stats.eventLoop);

    if (stale > 0) {
        ImGui::TextColored(getLevelColor(core::Log::Level::Warning), "%zu events or signals keep listeners whose owner is gone.", stale);
    }

    if (ImGui::CollapsingHeader("Profiler", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawProfiler(profiler);
    }

    if (ImGui::CollapsingHeader("Scenes", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawScenes(engine.getScenes());
    }

    if (ImGui::CollapsingHeader("Drawings", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawDrawings(plugin);
    }

    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawRendering(stats.rendering);
    }

    if (ImGui::CollapsingHeader("Memory, counters and GPU pools", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawResources();
    }

    if (ImGui::CollapsingHeader("Content")) {
        drawContent(engine.getContent());
    }

    if (ImGui::CollapsingHeader("Objects")) {
        drawObjects(stats.objects);
    }

    if (ImGui::CollapsingHeader("Signals and events")) {
        drawTopics("##events", events);
        drawTopics("##signals", signals);
    }

    if (!monitors.empty() && ImGui::CollapsingHeader("Monitors", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawMonitors();
    }

    if (ImGui::CollapsingHeader("Log", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawLog();
    }

    // The monitors hold the functions of the app that sample them, so the window lets go of them before the Lua state can close, while their vector keeps its storage.
    monitors.clear();
    DebugWindow::end();
    return open;
}

} // namespace haylen::debug
