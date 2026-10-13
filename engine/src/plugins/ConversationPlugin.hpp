#pragma once

#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Registers the `conversation` asset type for `.conversation` files, which are read on a worker thread, and installs the `haylen.conversation` module.
class ConversationPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "conversation";
    }
    void start(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::plugins
