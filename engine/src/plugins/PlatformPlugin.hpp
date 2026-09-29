#pragma once

#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Answers the engine.info and app.version platform calls, connects the replies and events of native code to the bridge of the app while it runs and installs haylen.platform.
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
