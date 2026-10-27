#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#include "platform/SurfaceCheck.hpp"
#endif

namespace haylen::platform::sdl {

class Runtime;

// The application of Apple platforms around the window that SDL opens. On macOS the runtime creates the application with the application delegate of Haylen before SDL starts, so SDL leaves the application, its delegate and its menus to the runtime. On iOS, iPadOS, Mac Catalyst, tvOS and visionOS, UIKit creates the application and scene delegate of Haylen, which derives from the one of SDL, so SDL runs the app inside the scene while the plugins receive every event of the application and its scenes.
class AppleApplication final {
  public:
    // Prepares the application before SDL runs it.
    static void prepare();

    // Places the views of the services over the window once it opened, and lets the application finish launching.
    static void finishLaunching(Runtime& runtime);

#if !TARGET_OS_OSX
    // Checks that the view of the window covers the window and that its drawable has the pixels of the bounds of the view, which debug builds do every frame.
    static void checkSurface(SurfaceCheck& check);
#endif

#if TARGET_OS_OSX
    // Whether the native field that edits a text field of the UI holds the keyboard, whose keys then belong to that field alone.
    [[nodiscard]] static bool isTextFieldFocused() noexcept;
#endif
};

} // namespace haylen::platform::sdl
