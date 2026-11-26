#include "modules/BuiltInModules.hpp"

#include <memory>

#include "haylen/lua/Modules.hpp"
#include "haylen/lua/modules/AssetsModule.hpp"
#include "modules/Animation2DModule.hpp"
#include "modules/AudioModule.hpp"
#include "modules/ContentModule.hpp"
#include "modules/ConversationModule.hpp"
#include "modules/CoreModule.hpp"
#include "modules/DebugModule.hpp"
#include "modules/Graphics2DModule.hpp"
#include "modules/HotReloadModule.hpp"
#include "modules/InputModule.hpp"
#include "modules/JobsModule.hpp"
#include "modules/LocalizationModule.hpp"
#include "modules/NativeModule.hpp"
#include "modules/Navigation2DModule.hpp"
#include "modules/NetModule.hpp"
#include "modules/Nodes2DModule.hpp"
#include "modules/Particles2DModule.hpp"
#include "modules/Physics2DModule.hpp"
#include "modules/PlatformModule.hpp"
#include "modules/Procedural2DModule.hpp"
#include "modules/Spatial2DModule.hpp"
#include "modules/Spatial3DModule.hpp"
#include "modules/StorageModule.hpp"
#include "modules/TiledModule.hpp"
#include "modules/UiModule.hpp"
#include "modules/VideoModule.hpp"

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
    modules.add(std::make_unique<Spatial3DModule>());
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
