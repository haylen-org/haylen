#include "TestPortWindow.hpp"

namespace haylen::platform {

bool TestPortWindow::queryOpen() const noexcept {
    return open;
}

void* TestPortWindow::queryHandle() const noexcept {
    return nullptr;
}

void* TestPortWindow::queryDisplay() const noexcept {
    return nullptr;
}

void* TestPortWindow::queryActivity() const noexcept {
    return nullptr;
}

math::Vec2 TestPortWindow::queryFramebufferSize() const noexcept {
    return kSize;
}

float TestPortWindow::queryScale() const noexcept {
    return 1.0F;
}

} // namespace haylen::platform
