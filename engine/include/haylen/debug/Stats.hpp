#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
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
    // Frame times in milliseconds over the recent frames, where the one percent low is the average of the slowest one percent of them, and the age of the input of the last frame that the window showed, where the GPU device reports it.
    struct Frame {
        double fps = 0.0;
        double milliseconds = 0.0;
        double average = 0.0;
        double minimum = 0.0;
        double maximum = 0.0;
        double onePercentLow = 0.0;
        std::uint32_t fixedSteps = 0;
        std::optional<double> inputAge;
    };

    // Bytes of Lua memory and estimated bytes of GPU and audio memory, where `batches` holds the instance buffers of static sprite batches and `buffers` the vertex, index and instance buffers the renderer streams its batches through every frame.
    struct Memory {
        std::size_t lua = 0;
        std::int64_t textures = 0;
        std::int64_t targets = 0;
        std::int64_t batches = 0;
        std::int64_t buffers = 0;
        std::int64_t sounds = 0;
        std::size_t gpuSpare = 0;
    };

    // The last poll of the event loop of Varn: the milliseconds it could take, whether anything ran, whether work remains and the milliseconds the loop can wait before it has work of its own, which is -1 when only work from outside can bring any.
    struct EventLoop {
        double budget = 0.0;
        bool ran = false;
        bool pending = false;
        long long idle = -1;
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

    // Takes the snapshot again into this one, in the storage that its lists hold, so a reader that keeps it allocates nothing once its lists grew.
    void update(core::Engine& engine, const graphics2d::Renderer::Stats& lastFrame);

    Frame frame;
    EventLoop eventLoop;
    graphics2d::Renderer::Stats rendering;
    Memory memory;
    Counts counts;
    std::vector<graphics::Device::Pool> pools;
    std::vector<audio::Mixer::BusStats> buses;
    std::vector<ObjectCounter::Snapshot> objects;

  private:
    void measureFrames(core::Engine& engine);
    [[nodiscard]] const ObjectCounter::Snapshot* findObject(std::string_view name) const noexcept;

    // The frame times of the history, slowest first, which the measure of the frames sorts.
    std::vector<float> frameTimes;
};

} // namespace haylen::debug
