#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "platform/NativeHost.hpp"

namespace haylen::platform {

class TestPortRuntime;

// The host of the test port, whose window has a fixed size and no cursor, clipboard or controllers, and whose GPU device the runtime makes.
class TestPortHost final : public NativeHost {
  public:
    explicit TestPortHost(TestPortRuntime& owner) noexcept : runtime(owner) {}

    [[nodiscard]] math::Vec2 getFramebufferSize() const noexcept override;
    [[nodiscard]] float getDpiScale() const noexcept override;
    [[nodiscard]] bool isFullscreen() const noexcept override;
    void setFullscreen(bool value) override;
    void setTitle(std::string_view value) override;
    void setCursor(Cursor value) override;
    void setCursorVisible(bool value) override;
    void setMouseLocked(bool value) override;
    void setClipboard(std::string_view value) override;
    [[nodiscard]] std::string getClipboard() const override;
    [[nodiscard]] bool canBeTransparent() const noexcept override;
    [[nodiscard]] TextInput& getTextInput() noexcept override;
    [[nodiscard]] std::unique_ptr<gpu::Device> createGpuDevice() override;
    [[nodiscard]] audio::Mixer::Setup getAudioSetup() const override;
    void closeWindow() override;
    void pollGamepads(std::span<input::GamepadState> gamepads) override;
    void rumbleGamepad(std::size_t index, float low, float high, float seconds) override;

  protected:
    void requestWindowClose() override;
    void setWindowFocusable(bool value) override;

  private:
    TestPortRuntime& runtime;
    std::string clipboard;
};

} // namespace haylen::platform
