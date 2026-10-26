#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>

#include <SDL3/SDL_mouse.h>

#include "platform/NativeHost.hpp"

namespace haylen::platform::sdl {

class Runtime;

// The host of the platforms that SDL runs, whose window SDL opens and whose frames the GPU device of the graphics API that the runtime chose draws.
class Host final : public platform::NativeHost {
  public:
    explicit Host(Runtime& owner) noexcept : runtime(owner) {}
    ~Host() override;

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
    [[nodiscard]] std::unique_ptr<gpu::Device> createGpuDevice() override;
    void closeWindow() override;

  protected:
    void requestWindowClose() override;
    void setWindowFocusable(bool value) override;

  private:
    static constexpr std::size_t kCursorCount = static_cast<std::size_t>(Cursor::NotAllowed) + 1;

    [[nodiscard]] static SDL_SystemCursor toSystemCursor(Cursor cursor) noexcept;

    Runtime& runtime;

    // The cursors of the system, which SDL makes the first time the app shows each one.
    std::array<SDL_Cursor*, kCursorCount> cursors{};
};

} // namespace haylen::platform::sdl
