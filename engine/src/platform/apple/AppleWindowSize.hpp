#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_VISION
#include "haylen/port/WindowSizeService.hpp"

namespace haylen::platform {

// The size of the window of a visionOS app, which the person resizes in the space around them and the app asks for through the geometry preferences of its scene. Everything runs on the main thread.
class AppleWindowSize final : public port::WindowSizeService {
  public:
    void requestSize(math::Vec2 points) override;
};

} // namespace haylen::platform
#endif
