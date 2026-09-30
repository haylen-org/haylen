#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/graphics/Device.hpp"

namespace haylen::core {
class Engine;
}

namespace haylen::debug {

// A snapshot of what the engine is doing, which the debug overlay shows and `haylen.debug.stats` returns.
class Stats final {
  public:
    // Frame times in milliseconds over the recent frames, where the one percent low is the average of the slowest one percent of them.
    struct Frame {
        double fps = 0.0;
        double milliseconds = 0.0;
        double average = 0.0;
        double minimum = 0.0;
        double maximum = 0.0;
        double onePercentLow = 0.0;
        std::uint32_t fixedSteps = 0;
    };

    // Bytes of Lua memory and estimated bytes of GPU and audio memory.
    struct Memory {
        std::size_t lua = 0;
        std::int64_t textures = 0;
        std::int64_t targets = 0;
        std::int64_t sounds = 0;
    };

    struct Counts {
        std::size_t scenes = 0;
        std::size_t tweens = 0;
        std::size_t timers = 0;
        std::size_t voices = 0;
        std::size_t assetsCached = 0;
        std::size_t assetsPending = 0;
        std::size_t sockets = 0;
        std::uint64_t bodies = 0;
        std::uint64_t contacts = 0;
        std::uint64_t particles = 0;
    };

    // Takes the snapshot with the renderer statistics of the last frame the renderer finished.
    [[nodiscard]] static Stats capture(core::Engine& engine, const graphics2d::Renderer::Stats& lastFrame);

    Frame frame;
    graphics2d::Renderer::Stats rendering;
    Memory memory;
    Counts counts;
    std::vector<graphics::Device::Pool> pools;
    std::vector<audio::Mixer::BusStats> buses;
    std::vector<ObjectCounter::Snapshot> objects;

  private:
    [[nodiscard]] static Frame measureFrames(core::Engine& engine);
    [[nodiscard]] static const ObjectCounter::Snapshot* findObject(const std::vector<ObjectCounter::Snapshot>& snapshots, std::string_view name) noexcept;
};

} // namespace haylen::debug
