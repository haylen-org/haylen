#include "modules/ConversationModule.hpp"

#include <memory>

#include "conversation/ConversationBinding.hpp"
#include "haylen/conversation/Conversation.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/modules/AssetsModule.hpp"

namespace haylen::lua::modules {

void ConversationModule::install(haylen::core::Engine& engine, lua_State* L) {
    lua::conversation::ConversationBinding::install(L);

    // clang-format off
    AssetsModule& assets = engine.getPlugin<ScriptPlugin>().getModule<AssetsModule>();
    assets.registerPusher("conversation", [](lua_State* state, const std::shared_ptr<void>& asset) {
        lua::conversation::ConversationBinding::pushConversation(state, std::static_pointer_cast<haylen::conversation::Conversation>(asset));
    });
    // clang-format on
}

} // namespace haylen::lua::modules
