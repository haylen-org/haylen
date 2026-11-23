#include "plugins/ConversationPlugin.hpp"

#include <memory>

#include "haylen/assets/Manager.hpp"
#include "haylen/conversation/Conversation.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JsonValidator.hpp"

namespace haylen::plugins {

void ConversationPlugin::start(core::Engine& engine) {
    // clang-format off
    engine.getAssets().registerType({
        .name = "conversation",
        .extensions = {".conversation"},
        .normalize = [](const core::Json& options) {
            core::JsonValidator::requireKnownOptions(options, {}, "Conversation options");
            return core::Json::object();
        },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            return std::make_shared<conversation::Conversation>(conversation::Conversation::parse(core::Json::parse(request.bytes.begin(), request.bytes.end())));
        },
        .finalize = [](std::shared_ptr<void> decoded, const assets::Manager::Request&) { return decoded; },
    });
    // clang-format on
}

} // namespace haylen::plugins
