#pragma once

#include "platform/NativeWindow.hpp"

namespace haylen::sokol {

// The window that `sokol_app` opens, as the system of each platform sees it.
class Window final : public platform::NativeWindow {
  protected:
    [[nodiscard]] bool queryOpen() const noexcept override;
    [[nodiscard]] void* queryHandle() const noexcept override;
    [[nodiscard]] void* queryDisplay() const noexcept override;
    [[nodiscard]] void* queryActivity() const noexcept override;
    [[nodiscard]] math::Vec2 queryFramebufferSize() const noexcept override;
    [[nodiscard]] float queryScale() const noexcept override;
};

} // namespace haylen::sokol
