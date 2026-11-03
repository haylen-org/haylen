#pragma once

#include "haylen/plugins/Plugin.hpp"
#include "platform/ReturnHold.hpp"

namespace haylen::platform {
class Host;
}

namespace haylen::plugins {

// Leaves an app that another app launched once the person holds the way back of the device, as `platform::ReturnHold` describes, and shows the progress of the hold over the app. An app that nothing launched has no way back to take, so the plugin follows nothing there.
class ReturnHoldPlugin final : public Plugin {
  public:
    explicit ReturnHoldPlugin(platform::Host& platformHost) noexcept : host(platformHost) {}

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "returnHold";
    }
    void event(core::Engine& engine, const platform::Event& event) override;
    void beginFrame(core::Engine& engine, float deltaSeconds) override;
    void renderOverlay(core::Engine& engine) override;

  private:
    static constexpr float kWidth = 320.0F;
    static constexpr float kHeight = 64.0F;
    static constexpr float kMargin = 16.0F;
    static constexpr float kTextSize = 18.0F;

    platform::Host& host;
    platform::ReturnHold hold;
};

} // namespace haylen::plugins
