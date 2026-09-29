#pragma once

#include <miniaudio.h>

#include <memory>
#include <vector>

#include "haylen/audio/Effect.hpp"

namespace haylen::audio {

// The effects between a source node and the node it feeds, such as a voice and its bus or a bus and its parent, in the order they were added. The audio thread follows a change of the wiring from its next block.
class EffectChain final {
  public:
    EffectChain(ma_engine& owner, ma_node* input, ma_node* output) noexcept : engine(owner), source(input), destination(output) {}

    // Releases every effect without connecting the source again, since the chain ends only together with its source.
    ~EffectChain();

    EffectChain(const EffectChain&) = delete;
    EffectChain& operator=(const EffectChain&) = delete;

    // Throws std::invalid_argument for an empty effect, an effect that already processes another bus or voice, or an effect of another mixer.
    void add(std::shared_ptr<Effect> effect);

    // Does nothing for an effect that is not in the chain.
    void remove(const Effect& effect);

    [[nodiscard]] const std::vector<std::shared_ptr<Effect>>& getEffects() const noexcept {
        return effects;
    }
    [[nodiscard]] bool empty() const noexcept {
        return effects.empty();
    }

    // Returns how long the chain keeps sounding after its source falls silent, which adds up along the chain.
    [[nodiscard]] float getTail() const noexcept;

  private:
    [[nodiscard]] static ma_node* nodeOf(const Effect& effect) noexcept;

    ma_engine& engine;
    ma_node* source;
    ma_node* destination;
    std::vector<std::shared_ptr<Effect>> effects;
};

} // namespace haylen::audio
