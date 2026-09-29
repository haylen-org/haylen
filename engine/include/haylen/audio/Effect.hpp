#pragma once

#include <cstdint>
#include <memory>

namespace haylen::audio {

class EffectChain;
class EffectNode;

// Processes the sound of one bus or one voice inside the mixer. Parameters change on the frame thread at any time, tweens included, and the audio thread applies them from the next block it mixes. An effect belongs to the mixer that first uses it and processes one bus or voice at a time.
class Effect {
  public:
    virtual ~Effect();

    Effect(const Effect&) = delete;
    Effect& operator=(const Effect&) = delete;

    [[nodiscard]] bool isAttached() const noexcept {
        return attached;
    }

    // Returns how many seconds the effect keeps sounding after its input falls silent, which is how long a finished voice lets its effects ring out.
    [[nodiscard]] virtual float getTail() const noexcept = 0;

  protected:
    Effect();

  private:
    friend class EffectChain;
    friend class EffectNode;

    // Prepares the effect for the mixer format and clears what it holds from earlier input, on the frame thread every time it joins a bus or voice, before the audio thread processes it.
    virtual void prepare(std::uint32_t sampleRate, std::uint32_t channels) = 0;

    // Processes interleaved frames on the audio thread.
    virtual void process(const float* input, float* output, std::uint32_t frames) noexcept = 0;

    std::unique_ptr<EffectNode> node;
    bool attached = false;
};

} // namespace haylen::audio
