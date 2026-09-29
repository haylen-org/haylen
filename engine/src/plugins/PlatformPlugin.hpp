#pragma once

#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Answers the engine.info and app.version platform calls and installs haylen.platform.
class PlatformPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "platform";
    }
    void start(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::plugins
