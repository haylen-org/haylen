#include "plugins/Nodes2DPlugin.hpp"

#include <memory>

#include "2d/nodes/Nodes2DLua.hpp"
#include "haylen/2d/nodes/Prefab.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JsonValidator.hpp"
#include "haylen/plugins/AssetsPlugin.hpp"

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
    engine.getPlugin<AssetsPlugin>().registerLuaPusher("prefab", [](lua_State* L, const std::shared_ptr<void>& asset) {
        nodes2d::Nodes2DLua::pushPrefab(L, std::static_pointer_cast<nodes2d::Prefab>(asset));
    });
    // clang-format on
}

void Nodes2DPlugin::installLua(core::Engine&, lua_State* L) {
    nodes2d::Nodes2DLua::install(L);
}

} // namespace haylen::plugins
