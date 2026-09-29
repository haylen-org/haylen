#pragma once

#include <miniaudio.h>

#include <cstdint>

namespace haylen::audio {

class Effect;

// The miniaudio node that runs an effect on the audio thread, with one input bus and one output bus in the channel count of the mixer. It keeps processing after its input falls silent, so echoes and reverbs ring out.
class EffectNode final {
  public:
    EffectNode(ma_node_graph& graph, Effect& target, std::uint32_t channels);
    ~EffectNode();

    EffectNode(const EffectNode&) = delete;
    EffectNode& operator=(const EffectNode&) = delete;

    [[nodiscard]] ma_node* get() noexcept {
        return this;
    }
    [[nodiscard]] const ma_node_graph* getGraph() const noexcept {
        return base.pNodeGraph;
    }

  private:
    static const ma_node_vtable kVtable;

    static void processFrames(ma_node* node, const float** framesIn, ma_uint32* frameCountIn, float** framesOut, ma_uint32* frameCountOut);

    // The base stays the first member of this standard-layout class, because miniaudio reaches the node through it.
    ma_node_base base{};
    Effect* effect;
};

} // namespace haylen::audio
