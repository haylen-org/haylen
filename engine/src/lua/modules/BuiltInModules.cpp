#include "lua/modules/BuiltInModules.hpp"

#include <memory>

#include "haylen/lua/Modules.hpp"
#include "haylen/lua/modules/AssetsModule.hpp"
#include "lua/modules/Animation2DModule.hpp"
#include "lua/modules/AudioModule.hpp"
#include "lua/modules/ContentModule.hpp"
#include "lua/modules/ConversationModule.hpp"
#include "lua/modules/CoreModule.hpp"
#include "lua/modules/DebugModule.hpp"
#include "lua/modules/Graphics2DModule.hpp"
#include "lua/modules/HotReloadModule.hpp"
#include "lua/modules/InputModule.hpp"
#include "lua/modules/JobsModule.hpp"
#include "lua/modules/LocalizationModule.hpp"
#include "lua/modules/NativeModule.hpp"
#include "lua/modules/Navigation2DModule.hpp"
#include "lua/modules/NetModule.hpp"
#include "lua/modules/Nodes2DModule.hpp"
#include "lua/modules/Particles2DModule.hpp"
#include "lua/modules/Physics2DModule.hpp"
#include "lua/modules/PlatformModule.hpp"
#include "lua/modules/Procedural2DModule.hpp"
#include "lua/modules/Spatial2DModule.hpp"
#include "lua/modules/StorageModule.hpp"
#include "lua/modules/TiledModule.hpp"
#include "lua/modules/UiModule.hpp"
#include "lua/modules/VideoModule.hpp"

namespace haylen::lua::modules {

void BuiltInModules::registerAll(Modules& modules) {
    modules.add(std::make_unique<CoreModule>());
    modules.add(std::make_unique<JobsModule>());
    modules.add(std::make_unique<InputModule>());
    modules.add(std::make_unique<Graphics2DModule>());
    modules.add(std::make_unique<AssetsModule>());
    modules.add(std::make_unique<ContentModule>());
    modules.add(std::make_unique<Animation2DModule>());
    modules.add(std::make_unique<Particles2DModule>());
    modules.add(std::make_unique<AudioModule>());
    modules.add(std::make_unique<VideoModule>());
    modules.add(std::make_unique<TiledModule>());
    modules.add(std::make_unique<Nodes2DModule>());
    modules.add(std::make_unique<Spatial2DModule>());
    modules.add(std::make_unique<Procedural2DModule>());
    modules.add(std::make_unique<Navigation2DModule>());
    modules.add(std::make_unique<LocalizationModule>());
    modules.add(std::make_unique<ConversationModule>());
    modules.add(std::make_unique<StorageModule>());
    modules.add(std::make_unique<DebugModule>());
    modules.add(std::make_unique<HotReloadModule>());
    modules.add(std::make_unique<Physics2DModule>());
    modules.add(std::make_unique<UiModule>());
    modules.add(std::make_unique<NetModule>());
    modules.add(std::make_unique<PlatformModule>());
    modules.add(std::make_unique<NativeModule>());
}

} // namespace haylen::lua::modules
