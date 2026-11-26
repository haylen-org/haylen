#include "haylen/debug/Stats.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <numeric>
#include <optional>

#include "haylen/assets/Manager.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/core/TimerScheduler.hpp"
#include "haylen/core/TweenManager.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "haylen/plugins/NetPlugin.hpp"

namespace haylen::debug {

const ObjectCounter::Snapshot* Stats::findObject(std::string_view name) const noexcept {
    const auto found = std::ranges::find(objects, name, &ObjectCounter::Snapshot::name);
    return found != objects.end() ? &*found : nullptr;
}

// The slowest frames come first, so the one percent low averages the head of the list.
void Stats::measureFrames(core::Engine& engine) {
    const Profiler& profiler = engine.getProfiler();
    const std::optional<double> inputAge = engine.getGraphics().getInputAge();
    frame = {.milliseconds = profiler.getLastFrameMilliseconds(), .fixedSteps = engine.getClock().getFixedStepCount(), .inputAge = inputAge ? std::optional<double>(*inputAge * 1000.0) : std::nullopt};
    profiler.readFrameHistory(frameTimes);
    if (frameTimes.empty()) {
        return;
    }

    std::ranges::sort(frameTimes, std::greater<>());
    const std::size_t slowest = std::max<std::size_t>(1, frameTimes.size() / 100);
    frame.average = std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0) / static_cast<double>(frameTimes.size());
    frame.fps = profiler.getFramesPerSecond();
    frame.minimum = frameTimes.back();
    frame.maximum = frameTimes.front();
    frame.onePercentLow = std::accumulate(frameTimes.begin(), frameTimes.begin() + static_cast<std::ptrdiff_t>(slowest), 0.0) / static_cast<double>(slowest);
}

Stats Stats::capture(core::Engine& engine, const graphics2d::Renderer::Stats& lastFrame) {
    Stats stats;
    stats.update(engine, lastFrame);
    return stats;
}

void Stats::update(core::Engine& engine, const graphics2d::Renderer::Stats& lastFrame) {
    measureFrames(engine);
    const core::Engine::EventLoopPoll& polled = engine.getEventLoopPoll();
    eventLoop = {.budget = std::chrono::duration<double, std::milli>(polled.budget).count(), .ran = polled.ran, .pending = polled.pending, .idle = polled.idleMilliseconds};
    rendering = lastFrame;
    engine.getGraphics().readPools(pools);
    engine.getAudio().readBusStats(buses);
    ObjectCounter::readAll(objects);

    memory.lua = engine.getPlugin<plugins::DebugPlugin>().getScriptMemory();
    // clang-format off
    const auto bytesOf = [this](std::string_view name) {
        const ObjectCounter::Snapshot* found = findObject(name);
        return found != nullptr ? found->bytes : 0;
    };
    const auto aliveOf = [this](std::string_view name) {
        const ObjectCounter::Snapshot* found = findObject(name);
        return found != nullptr ? found->alive : 0;
    };
    // clang-format on

    memory.textures = bytesOf("Texture");
    memory.targets = bytesOf("RenderTarget");
    memory.batches = bytesOf("StaticSpriteBatch");
    memory.buffers = bytesOf("RendererBuffers");
    memory.sounds = bytesOf("Sound");
    memory.gpuSpare = engine.getGraphics().getSpareBytes();

    counts.scenes = engine.getScenes().size();
    counts.tweens = engine.getTweens().size();
    counts.timers = engine.getTimers().size();
    counts.voices = engine.getAudio().getVoiceCount();
    counts.assetsCached = engine.getAssets().getCachedCount();
    counts.assetsPending = engine.getAssets().getPendingCount();
    counts.sockets = engine.getPlugin<plugins::NetPlugin>().getOpenSocketCount();
    counts.bodies = aliveOf("PhysicsBody2D") + aliveOf("PhysicsBody3D");
    counts.contacts = aliveOf("PhysicsContact2D");
    counts.particles = aliveOf("Particle");
}

} // namespace haylen::debug
