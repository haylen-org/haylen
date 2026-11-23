#include "haylen/plugins/Nodes2DPlugin.hpp"

#include <memory>

#include "haylen/2d/nodes/Prefab.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JsonValidator.hpp"

namespace haylen::plugins {

void Nodes2DPlugin::start(core::Engine& engine) {
    // clang-format off
    engine.getAssets().registerType({
        .name = "prefab",
        .extensions = {".prefab"},
        .normalize = [](const core::Json& options) {
            core::JsonValidator::requireKnownOptions(options, {}, "Prefab options");
            return core::Json::object();
        },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            return std::make_shared<nodes2d::Prefab>(nodes2d::Prefab::parse(core::Json::parse(request.bytes.begin(), request.bytes.end())));
        },
        .finalize = [](std::shared_ptr<void> decoded, const assets::Manager::Request&) { return decoded; },
    });
    // clang-format on
}

void Nodes2DPlugin::stop(core::Engine&) {
    trees.clear();
}

bool Nodes2DPlugin::isAnimating() const {
    return trees.any([](const nodes2d::NodeTree& tree) { return tree.hasLiveParticles(); });
}

void Nodes2DPlugin::track(const std::shared_ptr<nodes2d::NodeTree>& tree) {
    trees.add(tree);
}

} // namespace haylen::plugins
