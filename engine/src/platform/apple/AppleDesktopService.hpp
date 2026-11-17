#pragma once

#include <span>

#include "haylen/port/DesktopService.hpp"

namespace haylen::platform {

// The desktop of macOS, where the window of the app moves among other windows, through `AppleDesktop`.
class AppleDesktopService final : public port::DesktopService {
  public:
    void setWindowStyle(const WindowStyle& value) override;
    void setWindowBackground(const math::Color& value) override;
    void setWindowFrame(const math::Rect& value) override;
    void setMousePassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> regions) override;
    void startWindowDrag() override;
    void watchWindow() override;
    void updateWindow() override;
    [[nodiscard]] HaylenNativeWindow getNativeWindow() override;
};

} // namespace haylen::platform
