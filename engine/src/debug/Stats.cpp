#include "haylen/debug/Stats.hpp"

#include <lua.hpp>

#include <algorithm>
#include <functional>
#include <numeric>

#include "haylen/assets/Manager.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/core/TimerScheduler.hpp"
#include "haylen/core/TweenManager.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/plugins/NetPlugin.hpp"

namespace haylen::debug {

const ObjectCounter::Snapshot* Stats::findObject(const std::vector<ObjectCounter::Snapshot>& snapshots, std::string_view name) noexcept {
    const auto found = std::ranges::find(snapshots, name, &ObjectCounter::Snapshot::name);
    return found != snapshots.end() ? &*found : nullptr;
}

Stats::Frame Stats::measureFrames(core::Engine& engine) {
    const Profiler& profiler = engine.getProfiler();
    Frame measured{.milliseconds = profiler.getLastFrameMilliseconds(), .fixedSteps = engine.getClock().getFixedStepCount()};
    std::vector<float> times = profiler.getFrameHistory();
    if (times.empty()) {
        return measured;
    }

    // The slowest frames come first, so the one percent low averages the head of the list.
    std::ranges::sort(times, std::greater<>());
    const std::size_t slowest = std::max<std::size_t>(1, times.size() / 100);
    measured.average = std::accumulate(times.begin(), times.end(), 0.0) / static_cast<double>(times.size());
    measured.fps = measured.average > 0.0 ? 1000.0 / measured.average : 0.0;
    measured.minimum = times.back();
    measured.maximum = times.front();
    measured.onePercentLow = std::accumulate(times.begin(), times.begin() + static_cast<std::ptrdiff_t>(slowest), 0.0) / static_cast<double>(slowest);
    return measured;
}

Stats Stats::capture(core::Engine& engine, const graphics2d::Renderer::Stats& lastFrame) {
    Stats stats;
    stats.frame = measureFrames(engine);
    stats.rendering = lastFrame;
    stats.pools = engine.getGraphics().getPools();
    stats.buses = engine.getAudio().getBusStats();
    stats.objects = ObjectCounter::list();

    lua_State* L = engine.getLuaState();
    stats.memory.lua = static_cast<std::size_t>(lua_gc(L, LUA_GCCOUNT, 0)) * 1024 + static_cast<std::size_t>(lua_gc(L, LUA_GCCOUNTB, 0));
    // clang-format off
    const auto bytesOf = [&stats](std::string_view name) {
        const ObjectCounter::Snapshot* found = findObject(stats.objects, name);
        return found != nullptr ? found->bytes : 0;
    };
    const auto aliveOf = [&stats](std::string_view name) {
        const ObjectCounter::Snapshot* found = findObject(stats.objects, name);
        return found != nullptr ? found->alive : 0;
    };
    // clang-format on
    stats.memory.textures = bytesOf("Texture");
    stats.memory.targets = bytesOf("RenderTarget");
    stats.memory.sounds = bytesOf("Sound");

    stats.counts.scenes = engine.getScenes().size();
    stats.counts.tweens = engine.getTweens().size();
    stats.counts.timers = engine.getTimers().size();
    stats.counts.voices = engine.getAudio().getVoiceCount();
    stats.counts.assetsCached = engine.getAssets().getCachedCount();
    stats.counts.assetsPending = engine.getAssets().getPendingCount();
    stats.counts.sockets = engine.getPlugin<plugins::NetPlugin>().getOpenSocketCount();
    stats.counts.bodies = aliveOf("PhysicsBody");
    stats.counts.contacts = aliveOf("PhysicsContact");
    stats.counts.particles = aliveOf("Particle");
    return stats;
}

} // namespace haylen::debug
