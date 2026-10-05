#include "plugins/BuiltInPlugins.hpp"

#include <memory>

#include "haylen/plugins/AssetsPlugin.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "haylen/plugins/LocalizationPlugin.hpp"
#include "haylen/plugins/NetPlugin.hpp"
#include "haylen/plugins/PluginRegistry.hpp"
#include "haylen/plugins/StoragePlugin.hpp"
#include "haylen/plugins/TextPlugin.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "platform/Host.hpp"
#include "plugins/Animation2DPlugin.hpp"
#include "plugins/AudioPlugin.hpp"
#include "plugins/CorePlugin.hpp"
#include "plugins/Graphics2DPlugin.hpp"
#include "plugins/HotReloadPlugin.hpp"
#include "plugins/InputPlugin.hpp"
#include "plugins/JobsPlugin.hpp"
#include "plugins/NativePlugin.hpp"
#include "plugins/Navigation2DPlugin.hpp"
#include "plugins/Particles2DPlugin.hpp"
#include "plugins/Physics2DPlugin.hpp"
#include "plugins/PlatformPlugin.hpp"
#include "plugins/Procedural2DPlugin.hpp"
#include "plugins/Spatial2DPlugin.hpp"
#include "plugins/TiledPlugin.hpp"

namespace haylen::plugins {

void BuiltInPlugins::registerAll(PluginRegistry& registry, const platform::Host& host) {
    registry.add(std::make_unique<CorePlugin>());
    registry.add(std::make_unique<JobsPlugin>());
    registry.add(std::make_unique<InputPlugin>());
    registry.add(std::make_unique<Graphics2DPlugin>());
    registry.add(std::make_unique<AssetsPlugin>());
    registry.add(std::make_unique<TextPlugin>());
    registry.add(std::make_unique<Animation2DPlugin>());
    registry.add(std::make_unique<Particles2DPlugin>());
    registry.add(std::make_unique<AudioPlugin>());
    registry.add(std::make_unique<Physics2DPlugin>());
    registry.add(std::make_unique<TiledPlugin>());
    registry.add(std::make_unique<Spatial2DPlugin>());
    registry.add(std::make_unique<Procedural2DPlugin>());
    registry.add(std::make_unique<Navigation2DPlugin>());
    registry.add(std::make_unique<LocalizationPlugin>());
    registry.add(std::make_unique<StoragePlugin>());
    registry.add(std::make_unique<DebugPlugin>());
    registry.add(std::make_unique<HotReloadPlugin>(host.getDevelopmentSession()));
    registry.add(std::make_unique<UiPlugin>());
    registry.add(std::make_unique<NetPlugin>(host.getNetworkRequirement()));
    registry.add(std::make_unique<PlatformPlugin>());
    registry.add(std::make_unique<NativePlugin>());
}

} // namespace haylen::plugins
