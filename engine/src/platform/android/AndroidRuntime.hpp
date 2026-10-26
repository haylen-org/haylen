#pragma once

#include <android/choreographer.h>
#include <android/looper.h>
#include <android/native_window.h>
#include <game-activity/GameActivity.h>

#include <cstdint>
#include <thread>

#include "platform/android/AndroidFramePacer.hpp"
#include "platform/android/AndroidGraphics.hpp"
#include "platform/android/AndroidKeyClaims.hpp"
#include "platform/android/AndroidMessageQueue.hpp"
#include "platform/android/AndroidWindow.hpp"

namespace haylen::platform {

// Plays apps in a `GameActivity`. Each activity runs a frame thread of its own, which creates the player as it starts, starts the app when the activity first shows its window, draws a frame for each refresh of the display while the activity is resumed and has a window, ticks the app while the activity is paused, and ends the app when it quits or the activity is destroyed. The callbacks of the activity run on the UI thread and reach the frame thread through a queue, in order with the input, and a callback that ends the window or the activity returns only once the frame thread let go of it. A quit finishes the activity like the back button does, and the process lives on, so a new activity starts the runtime anew.
class AndroidRuntime final {
  public:
    explicit AndroidRuntime(GameActivity& gameActivity);
    AndroidRuntime(const AndroidRuntime&) = delete;
    AndroidRuntime& operator=(const AndroidRuntime&) = delete;
    ~AndroidRuntime() = default;

    // Starts the runtime of a new activity, which lasts until the activity is destroyed.
    static void create(GameActivity& gameActivity);

    [[nodiscard]] AndroidGraphics& getGraphics() noexcept {
        return graphics;
    }

    // End the app after the current frame and finish the activity, where a requested quit first tells the app that the window closes. They run on the frame thread.
    void requestQuit() noexcept {
        quitRequested = true;
    }

    void quit() noexcept {
        quitOrdered = true;
    }

  private:
    using PostFrameCallback = void (*)(AChoreographer* choreographer, AChoreographer_frameCallback64 callback, void* data);

    [[nodiscard]] static AndroidRuntime& from(GameActivity* gameActivity) noexcept {
        return *static_cast<AndroidRuntime*>(gameActivity->instance);
    }

    static void onResume(GameActivity* gameActivity);
    static void onPause(GameActivity* gameActivity);
    static void onDestroy(GameActivity* gameActivity);
    static void onWindowFocusChanged(GameActivity* gameActivity, bool focused);
    static void onNativeWindowCreated(GameActivity* gameActivity, ANativeWindow* nativeWindow);
    static void onNativeWindowResized(GameActivity* gameActivity, ANativeWindow* nativeWindow, std::int32_t width, std::int32_t height);
    static void onNativeWindowDestroyed(GameActivity* gameActivity, ANativeWindow* nativeWindow);
    static bool onTouchEvent(GameActivity* gameActivity, const GameActivityMotionEvent* event);
    static bool onKey(GameActivity* gameActivity, const GameActivityKeyEvent* event);

    static int onMessages(int descriptor, int events, void* data);
    static void onFrameCallback(std::int64_t frameNanoseconds, void* data);

    void run();
    void loop();
    void handleMessages();
    void handle(AndroidMessageQueue::Signal signal);
    void openWindow(const AndroidMessageQueue::Window& message);
    void closeWindow();
    void requestFrame();
    void drawFrame(double now);
    void replaceLostDevice();
    [[nodiscard]] int tick();
    void finishFrame();
    void end();
    [[nodiscard]] static double getTime() noexcept;

    GameActivity& activity;
    AndroidWindow window;
    AndroidGraphics graphics;
    AndroidMessageQueue messages;
    AndroidFramePacer pacer;

    // The keys that the app took from the views, which only the UI thread reaches.
    AndroidKeyClaims keys;

    // What only the frame thread reaches: its looper, the Choreographer that paces its frames from Android 10 on, and the state of the app.
    std::thread thread;
    ALooper* looper = nullptr;
    AChoreographer* choreographer = nullptr;
    PostFrameCallback postFrameCallback = nullptr;
    bool frameRequested = false;
    bool started = false;
    bool ended = false;
    bool stopping = false;
    bool quitRequested = false;
    bool quitOrdered = false;
};

} // namespace haylen::platform
