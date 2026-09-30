#pragma once

#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Connects the replies and events of native code and the answers of native dialogs to the app while it runs, and installs haylen.platform, haylen.system and haylen.dialogs.
class PlatformPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "platform";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::plugins
