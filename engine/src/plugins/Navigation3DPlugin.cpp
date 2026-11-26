#include "haylen/plugins/Navigation3DPlugin.hpp"

#include <memory>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JsonValidator.hpp"

namespace haylen::plugins {

void Navigation3DPlugin::start(core::Engine& engine) {
    // clang-format off
    engine.getAssets().registerType({
        .name = "navmesh",
        .extensions = {".navmesh"},
        .normalize = [](const core::Json& options) {
            core::JsonValidator::requireKnownOptions(options, {}, "Navigation mesh options");
            return core::Json::object();
        },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            return std::make_shared<navigation3d::NavMesh>(navigation3d::NavMesh::decode(request.bytes, request.path));
        },
        .finalize = [this](std::shared_ptr<void> decoded, const assets::Manager::Request&) {
            track(std::static_pointer_cast<navigation3d::NavMesh>(decoded));
            return decoded;
        },
    });
    // clang-format on
}

void Navigation3DPlugin::beginFrame(core::Engine& engine, float) {
    core::JobSystem& jobs = engine.getJobs();
    meshes.forEach([&jobs](navigation3d::NavMesh& mesh) { mesh.update(&jobs); });
}

void Navigation3DPlugin::stop(core::Engine&) {
    meshes.clear();
}

bool Navigation3DPlugin::isAnimating() const {
    return meshes.any([](const navigation3d::NavMesh& mesh) { return !mesh.isUpToDate(); });
}

void Navigation3DPlugin::track(const std::shared_ptr<navigation3d::NavMesh>& mesh) {
    meshes.add(mesh);
}

} // namespace haylen::plugins
