#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "haylen/platform/native/HaylenNative.h"

// The screens of the desktops: a native window over the window of the app with a title, a question and up to three buttons, Close, Decline and Confirm. The confirm screen, the fake store, the fake ads and the fake sign-in show their texts and results in it. The file `NativeDemoScreen.mm` opens it as a sheet of the window of the app on macOS, and `NativeDemoScreen.cpp` as a window that the window of the app owns on Windows and as a transient X11 window with a connection of its own on Linux.
class NativeDemoScreen final {
  public:
    // What a window shows and how its buttons end the screen, with its results as JSON texts. Confirm ends it with `confirmed` after it called `onConfirm`, when there is one, on the thread of the window. Decline, which shows only with a `declineLabel`, ends it with `declined`, or fails it with `closed` without one, and Close and the close box fail it with `closed`.
    struct Content {
        std::string title;
        std::string question;
        std::string confirmLabel;
        std::optional<std::string> declineLabel;
        std::string confirmed;
        std::optional<std::string> declined;
        std::string closed;
        std::function<void()> onConfirm;
    };

    // Opens the window of the screen on the frame thread, or ends the screen with a failure when the app has no window.
    static void open(const HaylenNativeApi& api, std::uint64_t screen, Content content);

    // Closes the window of a screen that the app gave up, which ends the screen with the code `cancelled`.
    static void cancel(std::uint64_t screen);

    // The window that shows, which the implementation of each system keeps in its own way and its window callbacks reach.
    struct Showing;
    [[nodiscard]] static Showing& getShowing();
};
