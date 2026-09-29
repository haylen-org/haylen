#include "debug/OverlayWindow.hpp"

#include <imgui.h>

#include <algorithm>
#include <format>

#include "core/SignalLua.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/debug/Profiler.hpp"

namespace haylen::debug {

ImVec4 OverlayWindow::getLevelColor(core::Log::Level level) noexcept {
    switch (level) {
    case core::Log::Level::Debug:
        return {0.6F, 0.65F, 0.75F, 1.0F};
    case core::Log::Level::Warning:
        return {0.97F, 0.8F, 0.44F, 1.0F};
    case core::Log::Level::Error:
        return {1.0F, 0.54F, 0.52F, 1.0F};
    case core::Log::Level::Info:
        break;
    }
    return {0.91F, 0.92F, 0.95F, 1.0F};
}

std::string OverlayWindow::formatBytes(std::int64_t bytes) {
    const auto value = static_cast<double>(bytes);
    if (value >= 1024.0 * 1024.0) {
        return std::format("{:.1f} MB", value / (1024.0 * 1024.0));
    }
    if (value >= 1024.0) {
        return std::format("{:.1f} KB", value / 1024.0);
    }
    return std::format("{} B", bytes);
}

void OverlayWindow::drawFrame(const Profiler& profiler, const Stats::Frame& frame) {
    ImGui::TextWrapped("%.0f FPS, %.2f ms average, %.2f ms last frame", frame.fps, frame.average, frame.milliseconds);
    ImGui::TextWrapped("Fastest %.2f ms, slowest %.2f ms, 1%% low %.2f ms, %u fixed steps", frame.minimum, frame.maximum, frame.onePercentLow, frame.fixedSteps);
    const std::vector<float> history = profiler.getFrameHistory();
    ImGui::PlotLines("##frames", history.data(), static_cast<int>(history.size()), 0, nullptr, 0.0F, 50.0F, {-1.0F, 80.0F});
}

void OverlayWindow::drawProfiler(const Profiler& profiler) {
    if (!ImGui::BeginTable("##scopes", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        return;
    }
    ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch, 3.0F);
    ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableSetupColumn("calls", ImGuiTableColumnFlags_WidthStretch, 1.0F);
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
    ImGui::TextWrapped("Uploaded %s", formatBytes(static_cast<std::int64_t>(rendering.uploadedBytes)).c_str());
}

void OverlayWindow::drawResources(const Stats& stats) {
    ImGui::TextWrapped("Lua %s, textures %s, render targets %s, sounds %s", formatBytes(static_cast<std::int64_t>(stats.memory.lua)).c_str(), formatBytes(stats.memory.textures).c_str(), formatBytes(stats.memory.targets).c_str(), formatBytes(stats.memory.sounds).c_str());
    const Stats::Counts& counts = stats.counts;
    ImGui::TextWrapped("Scenes %zu, tweens %zu, timers %zu, voices %zu, sockets %zu", counts.scenes, counts.tweens, counts.timers, counts.voices, counts.sockets);
    ImGui::TextWrapped("Assets cached %zu, loading %zu", counts.assetsCached, counts.assetsPending);
    std::string voices = "Voices by bus:";
    for (const audio::Mixer::BusStats& bus : stats.buses) {
        voices += std::format(" {} {}{}", bus.name, bus.voices, bus.processing ? "" : " (paused)");
    }
    ImGui::TextWrapped("%s", voices.c_str());
    ImGui::TextWrapped("Bodies %llu, contacts %llu, particles %llu", static_cast<unsigned long long>(counts.bodies), static_cast<unsigned long long>(counts.contacts), static_cast<unsigned long long>(counts.particles));

    // A pool that fills up makes the next GPU object fail, so the bar turns red past nine tenths.
    for (const graphics::Device::Pool& pool : stats.pools) {
        const float fraction = pool.size > 0 ? static_cast<float>(pool.used) / static_cast<float>(pool.size) : 0.0F;
        const std::string label = std::format("{} {} / {}", pool.name, pool.used, pool.size);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, fraction > 0.9F ? ImVec4(0.9F, 0.35F, 0.3F, 1.0F) : ImVec4(0.35F, 0.6F, 0.9F, 1.0F));
        ImGui::ProgressBar(fraction, {-1.0F, 0.0F}, label.c_str());
        ImGui::PopStyleColor();
    }
}

void OverlayWindow::drawObjects(const std::vector<ObjectCounter::Snapshot>& objects) {
    if (!ImGui::BeginTable("##objects", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        return;
    }
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 3.0F);
    ImGui::TableSetupColumn("alive", ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableSetupColumn("created", ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableSetupColumn("destroyed", ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableSetupColumn("memory", ImGuiTableColumnFlags_WidthStretch, 1.2F);
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
            ImGui::TextUnformatted(formatBytes(object.bytes).c_str());
        }
    }
    ImGui::EndTable();
}

std::size_t OverlayWindow::countStale(const std::vector<core::EventBus::Topic>& topics) noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(topics, [](const core::EventBus::Topic& topic) { return topic.stale > 0; }));
}

void OverlayWindow::drawTopics(const char* id, const std::vector<core::EventBus::Topic>& topics) {
    if (topics.empty() || !ImGui::BeginTable(id, 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        return;
    }
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 3.0F);
    ImGui::TableSetupColumn("listeners", ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableSetupColumn("emits", ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableSetupColumn("owner gone", ImGuiTableColumnFlags_WidthStretch, 1.0F);
    ImGui::TableHeadersRow();
    for (const core::EventBus::Topic& topic : topics) {
        ImGui::TableNextRow();
        if (topic.stale > 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, getLevelColor(core::Log::Level::Warning));
        }
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(topic.name.empty() ? "(unnamed)" : topic.name.c_str());
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

void OverlayWindow::drawMonitors(const std::vector<std::shared_ptr<Monitor>>& monitors) {
    for (const std::shared_ptr<Monitor>& monitor : monitors) {
        const std::vector<float> history = monitor->getHistory();
        if (history.empty()) {
            continue;
        }
        const auto [lowest, highest] = std::ranges::minmax(history);
        const std::string label = std::format("{} {:.3g}", monitor->getName(), monitor->getValue());
        ImGui::PlotLines(("##" + monitor->getName()).c_str(), history.data(), static_cast<int>(history.size()), 0, label.c_str(), lowest, std::max(highest, lowest + 1.0F), {-1.0F, 48.0F});
    }
}

void OverlayWindow::drawLog(const std::vector<LogLine>& log) {
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

bool OverlayWindow::draw(core::Engine& engine, const Stats& stats, const std::vector<std::shared_ptr<Monitor>>& monitors, const std::vector<LogLine>& log) {
    bool open = true;
    if (!ImGui::Begin("Haylen debug", &open)) {
        ImGui::End();
        return open;
    }

    // Listeners whose owner died stay connected until their topic emits again, so the warning shows even while their section is closed.
    const Profiler& profiler = engine.getProfiler();
    const std::vector<core::EventBus::Topic> events = engine.getEvents().getTopics();
    const std::vector<core::EventBus::Topic> signals = core::SignalLua::getNamedSignals(engine.getLuaState());
    const std::size_t stale = countStale(events) + countStale(signals);
    drawFrame(profiler, stats.frame);
    if (stale > 0) {
        ImGui::TextColored(getLevelColor(core::Log::Level::Warning), "%zu events or signals keep listeners whose owner is gone.", stale);
    }
    if (ImGui::CollapsingHeader("Profiler", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawProfiler(profiler);
    }
    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawRendering(stats.rendering);
    }
    if (ImGui::CollapsingHeader("Memory, counters and GPU pools", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawResources(stats);
    }
    if (ImGui::CollapsingHeader("Objects")) {
        drawObjects(stats.objects);
    }
    if (ImGui::CollapsingHeader("Signals and events")) {
        drawTopics("##events", events);
        drawTopics("##signals", signals);
    }
    if (!monitors.empty() && ImGui::CollapsingHeader("Monitors", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawMonitors(monitors);
    }
    if (ImGui::CollapsingHeader("Log", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawLog(log);
    }
    ImGui::End();
    return open;
}

} // namespace haylen::debug
