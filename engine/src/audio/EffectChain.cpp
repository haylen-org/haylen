#include "audio/EffectChain.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

#include "audio/EffectNode.hpp"

namespace haylen::audio {

EffectChain::~EffectChain() {
    for (const std::shared_ptr<Effect>& effect : effects) {
        ma_node_detach_output_bus(nodeOf(*effect), 0);
        effect->attached = false;
    }
}

void EffectChain::add(std::shared_ptr<Effect> effect) {
    if (!effect) {
        throw std::invalid_argument("An audio effect is required.");
    }
    if (effect->attached) {
        throw std::invalid_argument("The audio effect already processes another bus or voice.");
    }

    // An effect joins the node graph of the first mixer that uses it, prepared for its format.
    ma_node_graph& graph = *ma_engine_get_node_graph(&engine);
    if (!effect->node) {
        const std::uint32_t channels = ma_engine_get_channels(&engine);
        effect->prepare(ma_engine_get_sample_rate(&engine), channels);
        effect->node = std::make_unique<EffectNode>(graph, *effect, channels);
    } else if (effect->node->getGraph() != &graph) {
        throw std::invalid_argument("The audio effect belongs to another mixer.");
    }

    ma_node* previous = effects.empty() ? source : nodeOf(*effects.back());
    ma_node_attach_output_bus(nodeOf(*effect), 0, destination, 0);
    ma_node_attach_output_bus(previous, 0, nodeOf(*effect), 0);
    effect->attached = true;
    effects.push_back(std::move(effect));
}

void EffectChain::remove(const Effect& effect) {
    const auto found = std::ranges::find_if(effects, [&effect](const std::shared_ptr<Effect>& candidate) { return candidate.get() == &effect; });
    if (found == effects.end()) {
        return;
    }

    const auto position = static_cast<std::size_t>(found - effects.begin());
    ma_node* previous = position == 0 ? source : nodeOf(*effects[position - 1]);
    ma_node* next = position + 1 < effects.size() ? nodeOf(*effects[position + 1]) : destination;
    ma_node_attach_output_bus(previous, 0, next, 0);
    ma_node_detach_output_bus(nodeOf(effect), 0);
    (*found)->attached = false;
    effects.erase(found);
}

float EffectChain::getTail() const noexcept {
    float tail = 0.0F;
    for (const std::shared_ptr<Effect>& effect : effects) {
        tail += effect->getTail();
    }
    return tail;
}

ma_node* EffectChain::nodeOf(const Effect& effect) noexcept {
    return effect.node->get();
}

} // namespace haylen::audio
