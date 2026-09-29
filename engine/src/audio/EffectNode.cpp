#include "audio/EffectNode.hpp"

#include <stdexcept>

#include "haylen/audio/Effect.hpp"

namespace haylen::audio {

const ma_node_vtable EffectNode::kVtable{&EffectNode::processFrames, nullptr, 1, 1, MA_NODE_FLAG_CONTINUOUS_PROCESSING};

EffectNode::EffectNode(ma_node_graph& graph, Effect& target, std::uint32_t channels) : effect(&target) {
    ma_node_config config = ma_node_config_init();
    config.vtable = &kVtable;
    config.pInputChannels = &channels;
    config.pOutputChannels = &channels;
    if (ma_node_init(&graph, &config, nullptr, this) != MA_SUCCESS) {
        throw std::runtime_error("An audio effect could not join the mixer.");
    }
}

EffectNode::~EffectNode() {
    ma_node_uninit(this, nullptr);
}

void EffectNode::processFrames(ma_node* node, const float** framesIn, ma_uint32*, float** framesOut, ma_uint32* frameCountOut) {
    static_cast<EffectNode*>(node)->effect->process(framesIn[0], framesOut[0], *frameCountOut);
}

} // namespace haylen::audio
