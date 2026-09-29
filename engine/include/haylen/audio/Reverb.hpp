#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

#include "haylen/audio/Effect.hpp"

namespace haylen::audio {

class Freeverb;

// A room reverb after Freeverb, the Schroeder and Moorer design of parallel comb filters and series allpass filters. The room size sets how long the reverb lasts, damping softens its high frequencies, width spreads it across the stereo field, and the reverb mixes at the wet level over the input at the dry level. Every parameter lies between 0 and 1, and the reverb fills the first two channels of the mix.
class Reverb final : public Effect {
  public:
    struct Settings {
        float roomSize = 0.5F;
        float damping = 0.5F;
        float width = 1.0F;
        float wet = 0.33F;
        float dry = 1.0F;
    };

    explicit Reverb(const Settings& settings);
    ~Reverb() override;

    [[nodiscard]] float getRoomSize() const noexcept {
        return roomSize.load(std::memory_order_relaxed);
    }
    void setRoomSize(float value);
    [[nodiscard]] float getDamping() const noexcept {
        return damping.load(std::memory_order_relaxed);
    }
    void setDamping(float value);
    [[nodiscard]] float getWidth() const noexcept {
        return width.load(std::memory_order_relaxed);
    }
    void setWidth(float value);
    [[nodiscard]] float getWet() const noexcept {
        return wet.load(std::memory_order_relaxed);
    }
    void setWet(float value);
    [[nodiscard]] float getDry() const noexcept {
        return dry.load(std::memory_order_relaxed);
    }
    void setDry(float value);

    [[nodiscard]] float getTail() const noexcept override;

  private:
    static void requireUnit(float value, const char* name);

    void prepare(std::uint32_t rate, std::uint32_t count) override;
    void process(const float* input, float* output, std::uint32_t frames) noexcept override;
    void changed() noexcept;

    std::atomic<float> roomSize;
    std::atomic<float> damping;
    std::atomic<float> width;
    std::atomic<float> wet;
    std::atomic<float> dry;
    std::atomic<std::uint32_t> version{1};

    std::uint32_t appliedVersion = 0;
    std::uint32_t channels = 0;
    std::unique_ptr<Freeverb> model;
};

} // namespace haylen::audio
