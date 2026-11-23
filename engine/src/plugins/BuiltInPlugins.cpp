#include "plugins/BuiltInPlugins.hpp"

#include <memory>

#include "haylen/plugins/DebugPlugin.hpp"
#include "haylen/plugins/LocalizationPlugin.hpp"
#include "haylen/plugins/NetPlugin.hpp"
#include "haylen/plugins/PluginRegistry.hpp"
#include "haylen/plugins/StoragePlugin.hpp"
#include "haylen/plugins/TextPlugin.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/plugins/VideoPlugin.hpp"
#include "platform/Host.hpp"
#include "plugins/Animation2DPlugin.hpp"
#include "plugins/AudioPlugin.hpp"
#include "plugins/ContentPlugin.hpp"
#include "plugins/ConversationPlugin.hpp"
#include "plugins/HotReloadPlugin.hpp"
#include "plugins/Nodes2DPlugin.hpp"
#include "plugins/Particles2DPlugin.hpp"
#include "plugins/Physics2DPlugin.hpp"
#include "plugins/PlatformPlugin.hpp"
#include "plugins/ReturnHoldPlugin.hpp"
#include "plugins/TiledPlugin.hpp"

namespace haylen::plugins {

void BuiltInPlugins::registerAll(PluginRegistry& registry, platform::Host& host) {
    registry.add(std::make_unique<ContentPlugin>());
    registry.add(std::make_unique<TextPlugin>(host));
    registry.add(std::make_unique<Animation2DPlugin>());
    registry.add(std::make_unique<Particles2DPlugin>());
    registry.add(std::make_unique<AudioPlugin>());
    registry.add(std::make_unique<VideoPlugin>(host));
    registry.add(std::make_unique<TiledPlugin>());
    registry.add(std::make_unique<Nodes2DPlugin>());
    registry.add(std::make_unique<LocalizationPlugin>());
    registry.add(std::make_unique<ConversationPlugin>());
    registry.add(std::make_unique<StoragePlugin>());
    registry.add(std::make_unique<DebugPlugin>(host.getDevelopmentSession()));
    registry.add(std::make_unique<HotReloadPlugin>(host.getDevelopmentSession()));
    registry.add(std::make_unique<Physics2DPlugin>());
    registry.add(std::make_unique<UiPlugin>());
    registry.add(std::make_unique<ReturnHoldPlugin>(host));
    registry.add(std::make_unique<NetPlugin>(host.getNetworkRequirement(), host.getBlockedReason()));
    registry.add(std::make_unique<PlatformPlugin>());
}

} // namespace haylen::plugins
