// The confirm screen of the desktops: a native window over the window of the app with a question and the buttons Confirm, Decline and Close. Confirm and Decline end the screen with {confirmed, via, language}, and Close, the close box and a cancel of the app end it with the code cancelled. NativeDemoScreen.m opens it as a sheet of the window of the app on macOS, and NativeDemoScreen.c as a window that the window of the app owns on Windows and as a transient X11 window with a connection of its own on Linux.
#pragma once

#include <stdint.h>

#include "haylen/platform/native/HaylenNative.h"

// Opens the window of the screen on the frame thread, or ends the screen with a failure when the app has no window.
void native_demo_screen_open(const HaylenNativeApi* api, uint64_t screen, const char* title, const char* question);

// Closes the window of a screen that the app gave up, which ends the screen with the code cancelled.
void native_demo_screen_cancel(uint64_t screen);
