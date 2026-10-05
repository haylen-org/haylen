// The screens of the desktops: a native window over the window of the app with a title, a question and up to three buttons, Close, Decline and Confirm. The confirm screen, the fake store, the fake ads and the fake sign-in show their texts and results in it. The file `NativeDemoScreen.m` opens it as a sheet of the window of the app on macOS, and `NativeDemoScreen.c` as a window that the window of the app owns on Windows and as a transient X11 window with a connection of its own on Linux.
#pragma once

#include <stdint.h>

#include "haylen/platform/native/HaylenNative.h"

// What a window shows and how its buttons end the screen, as JSON texts. Confirm ends it with `confirmed` after it called `onConfirm`, when there is one, on the thread of the window. Decline, which shows only with a `declineLabel`, ends it with `declined`, or fails it with `closed` when `declined` is null, and Close and the close box fail it with `closed`. The window keeps copies of the texts.
typedef struct NativeDemoScreenContent {
    const char* title;
    const char* question;
    const char* confirmLabel;
    const char* declineLabel;
    const char* confirmed;
    const char* declined;
    const char* closed;
    void (*onConfirm)(void);
} NativeDemoScreenContent;

// Opens the window of the screen on the frame thread, or ends the screen with a failure when the app has no window.
void native_demo_screen_open(const HaylenNativeApi* api, uint64_t screen, const NativeDemoScreenContent* content);

// Closes the window of a screen that the app gave up, which ends the screen with the code `cancelled`.
void native_demo_screen_cancel(uint64_t screen);
