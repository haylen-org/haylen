#pragma once

#include <string_view>

namespace haylen::platform {

// The OpenGL ES version that the context of an Android app asks for, the newest one the device declares in the system property `ro.opengles.version`. Emulators create a context of exactly the version an app asks for, while every context reports the newest version of the device, so `sokol_gfx` would query the features of that version in a context without them. It compiles on every platform, so the tests of every host check it.
class AndroidGlesVersion final {
  public:
    int major = 3;
    int minor = 0;

    // Takes the value of `ro.opengles.version`, with the major version in its upper 16 bits and the minor version in its lower 16 bits.
    [[nodiscard]] static AndroidGlesVersion fromProperty(std::string_view value);

#if defined(__ANDROID__)
    [[nodiscard]] static AndroidGlesVersion read();
#endif
};

} // namespace haylen::platform
