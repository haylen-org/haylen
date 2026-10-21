#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "platform/NativeHost.hpp"
#include "sokol_app.h"

namespace haylen::sokol {

// The host of the Sokol backend, whose window `sokol_app` opens and whose frames `sokol_gfx` draws.
class Host final : public platform::NativeHost {
  public:
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
    [[nodiscard]] static sapp_mouse_cursor toSokolCursor(Cursor cursor) noexcept;
};

} // namespace haylen::sokol
