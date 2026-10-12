#pragma once

#include <memory>
#include <string_view>

#include "haylen/plugins/Plugin.hpp"

namespace haylen::content {
class UpdateScreen;
}

namespace haylen::plugins {

// Wires the content manager into the engine: it starts it, which holds the start of the app while its required content downloads, drives its downloads every frame, draws the update screen while the app waits, and installs `haylen.content`.
class ContentPlugin final : public Plugin {
  public:
    ContentPlugin();
    ~ContentPlugin() override;

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "content";
    }

    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;
    void event(core::Engine& engine, const platform::Event& event) override;
    void update(core::Engine& engine, float deltaSeconds) override;
    void renderOverlay(core::Engine& engine) override;
    [[nodiscard]] bool isCapturingBack() const override;

  private:
    std::unique_ptr<content::UpdateScreen> screen;
};

} // namespace haylen::plugins
