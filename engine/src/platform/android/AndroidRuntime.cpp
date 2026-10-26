#include "platform/android/AndroidRuntime.hpp"

#include <android/input.h>
#include <dlfcn.h>
#include <jni.h>
#include <pthread.h>

#include <ctime>
#include <exception>
#include <memory>
#include <variant>

#include "gpu/DeviceFactory.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Log.hpp"
#include "platform/Player.hpp"
#include "platform/android/AndroidHost.hpp"
#include "platform/android/AndroidInputEvents.hpp"

namespace haylen::platform {

AndroidRuntime::AndroidRuntime(GameActivity& gameActivity) : activity(gameActivity), window(gameActivity), graphics(window) {}

// The callbacks that the runtime leaves out belong to the Java side of `HaylenActivity`, which reports the insets, the density, the orientation, the theme and the warnings of low memory itself, and keeps the state that outlives the process.
void AndroidRuntime::create(GameActivity& gameActivity) {
    auto runtime = std::make_unique<AndroidRuntime>(gameActivity);
    GameActivityCallbacks& callbacks = *gameActivity.callbacks;
    callbacks.onResume = &onResume;
    callbacks.onPause = &onPause;
    callbacks.onDestroy = &onDestroy;
    callbacks.onWindowFocusChanged = &onWindowFocusChanged;
    callbacks.onNativeWindowCreated = &onNativeWindowCreated;
    callbacks.onNativeWindowResized = &onNativeWindowResized;
    callbacks.onNativeWindowDestroyed = &onNativeWindowDestroyed;
    callbacks.onTouchEvent = &onTouchEvent;
    callbacks.onKeyDown = &onKey;
    callbacks.onKeyUp = &onKey;

    AndroidRuntime& started = *runtime;
    gameActivity.instance = runtime.release();
    started.thread = std::thread(&AndroidRuntime::run, &started);
}

void AndroidRuntime::onResume(GameActivity* gameActivity) {
    from(gameActivity).messages.push(AndroidMessageQueue::Signal::Resumed);
}

void AndroidRuntime::onPause(GameActivity* gameActivity) {
    from(gameActivity).messages.push(AndroidMessageQueue::Signal::Paused);
}

// GameActivity frees itself once this returns, so the frame thread ends the app and itself first.
void AndroidRuntime::onDestroy(GameActivity* gameActivity) {
    const std::unique_ptr<AndroidRuntime> runtime(&from(gameActivity));
    gameActivity->instance = nullptr;
    runtime->messages.push(AndroidMessageQueue::Signal::Destroyed);
    runtime->thread.join();
}

// The focus of the window changes while a dialog or the notification shade covers the app, in order with the input, so the axes of controllers that it releases never come back from an event before it.
void AndroidRuntime::onWindowFocusChanged(GameActivity* gameActivity, bool focused) {
    from(gameActivity).messages.push(focused ? AndroidMessageQueue::Signal::FocusGained : AndroidMessageQueue::Signal::FocusLost);
}

// A new window has the size of its view until the runtime sizes its buffers.
void AndroidRuntime::onNativeWindowCreated(GameActivity* gameActivity, ANativeWindow* nativeWindow) {
    from(gameActivity).messages.push(AndroidMessageQueue::Window{.window = nativeWindow, .width = ANativeWindow_getWidth(nativeWindow), .height = ANativeWindow_getHeight(nativeWindow)});
}

void AndroidRuntime::onNativeWindowResized(GameActivity* gameActivity, ANativeWindow* nativeWindow, std::int32_t width, std::int32_t height) {
    from(gameActivity).messages.push(AndroidMessageQueue::Window{.window = nativeWindow, .width = width, .height = height});
}

// Android destroys the window once this returns, so the frame thread lets go of it first.
void AndroidRuntime::onNativeWindowDestroyed(GameActivity* gameActivity, ANativeWindow*) {
    AndroidRuntime& runtime = from(gameActivity);
    runtime.messages.waitHandled(runtime.messages.push(AndroidMessageQueue::Signal::WindowDestroyed));
}

// The history of a motion event lives in buffers that GameActivity reuses for the next event, so the copy keeps only its current axes.
bool AndroidRuntime::onTouchEvent(GameActivity* gameActivity, const GameActivityMotionEvent* event) {
    GameActivityMotionEvent copy = *event;
    copy.historySize = 0;
    copy.historicalEventTimesMillis = nullptr;
    copy.historicalEventTimesNanos = nullptr;
    copy.historicalAxisValues = nullptr;
    from(gameActivity).messages.push(copy);
    return true;
}

// The UI thread answers at once whether the app takes a key, and a key that the app leaves goes on to the views and the system, such as back, which reaches the app through the back callback of the activity.
bool AndroidRuntime::onKey(GameActivity* gameActivity, const GameActivityKeyEvent* event) {
    AndroidKeyClaims::Action action = AndroidKeyClaims::Action::Release;
    if (event->action == AKEY_EVENT_ACTION_DOWN) {
        action = event->repeatCount == 0 ? AndroidKeyClaims::Action::Press : AndroidKeyClaims::Action::Repeat;
    } else if (event->action != AKEY_EVENT_ACTION_UP) {
        return false;
    }

    AndroidRuntime& runtime = from(gameActivity);
    if (!runtime.keys.take(event->keyCode, action, AndroidInputEvents::wantsKey(*event))) {
        return false;
    }
    runtime.messages.push(*event);
    return true;
}

int AndroidRuntime::onMessages(int, int, void* data) {
    static_cast<AndroidRuntime*>(data)->handleMessages();
    return 1;
}

// The Choreographer stamps the frame with the monotonic clock, which the ticks of a paused app read too.
void AndroidRuntime::onFrameCallback(std::int64_t frameNanoseconds, void* data) {
    AndroidRuntime& runtime = *static_cast<AndroidRuntime*>(data);
    runtime.frameRequested = false;
    if (runtime.stopping || !runtime.pacer.isDrawing()) {
        return;
    }
    runtime.requestFrame();
    runtime.drawFrame(static_cast<double>(frameNanoseconds) / 1.0e9);
}

// The player reads the bundled package through the activity, so the window is installed first. Devices before Android 10 lack `AChoreographer_postFrameCallback64` and draw a frame per turn of the loop instead, which the swap of each frame paces.
void AndroidRuntime::run() {
    pthread_setname_np(pthread_self(), "haylen-frame");
    looper = ALooper_prepare(0);
    ALooper_addFd(looper, messages.getDescriptor(), ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, &onMessages, this);
    postFrameCallback = reinterpret_cast<PostFrameCallback>(dlsym(RTLD_DEFAULT, "AChoreographer_postFrameCallback64"));
    if (postFrameCallback != nullptr) {
        choreographer = AChoreographer_getInstance();
    }

    NativeWindow::install(&window);
    window.setRunning(true);
    Player::create(0, nullptr, std::make_unique<AndroidHost>(*this));
    const core::AppConfig::Window& config = Player::getOpeningConfig().window;
    window.setHighDpi(config.highDpi);
    graphics.configure(Player::getGraphicsApis(), {.sampleCount = config.sampleCount, .vsync = config.vsync});

    loop();

    end();
    graphics.release();
    window.setRunning(false);
    NativeWindow::install(nullptr);
    ALooper_removeFd(looper, messages.getDescriptor());
}

void AndroidRuntime::loop() {
    while (!stopping) {
        if (choreographer != nullptr) {
            requestFrame();
            ALooper_pollOnce(tick(), nullptr, nullptr, nullptr);
            continue;
        }
        if (pacer.isDrawing()) {
            drawFrame(getTime());
        }
        ALooper_pollOnce(pacer.isDrawing() ? 0 : tick(), nullptr, nullptr, nullptr);
    }
}

void AndroidRuntime::handleMessages() {
    messages.quiet();
    AndroidMessageQueue::Message message;
    std::uint64_t handled = 0;
    while (messages.pop(message)) {
        handled = message.sequence;
        if (const auto* signal = std::get_if<AndroidMessageQueue::Signal>(&message.payload)) {
            handle(*signal);
        } else if (const auto* change = std::get_if<AndroidMessageQueue::Window>(&message.payload)) {
            openWindow(*change);
        } else if (const auto* motion = std::get_if<GameActivityMotionEvent>(&message.payload)) {
            AndroidInputEvents::dispatchMotion(*motion);
        } else if (const auto* key = std::get_if<GameActivityKeyEvent>(&message.payload)) {
            AndroidInputEvents::dispatchKey(*key);
        }
    }
    messages.markHandled(handled);
}

// The app hears every change of the lifecycle, also before it starts, so it starts in the state that the activity is in.
void AndroidRuntime::handle(AndroidMessageQueue::Signal signal) {
    switch (signal) {
    case AndroidMessageQueue::Signal::Resumed:
        pacer.setResumed(true);
        Player::handleEvent({.type = Event::Type::Resumed});
        break;
    case AndroidMessageQueue::Signal::Paused:
        pacer.setResumed(false);
        Player::handleEvent({.type = Event::Type::Suspended});
        break;
    case AndroidMessageQueue::Signal::FocusGained:
        Player::handleEvent({.type = Event::Type::FocusGained});
        break;
    case AndroidMessageQueue::Signal::FocusLost:
        Player::handleEvent({.type = Event::Type::FocusLost});
        break;
    case AndroidMessageQueue::Signal::WindowDestroyed:
        closeWindow();
        break;
    case AndroidMessageQueue::Signal::Destroyed:
        stopping = true;
        break;
    }
}

// A window that the app cannot draw into ends the app, which explains why in the log.
void AndroidRuntime::openWindow(const AndroidMessageQueue::Window& message) {
    const bool resized = window.getNative() == message.window;
    window.open(*message.window, message.width, message.height);
    pacer.setWindow(true);
    try {
        if (resized) {
            graphics.resizeWindow(Player::getGpuDevice());
        } else {
            graphics.attachWindow(Player::getGpuDevice());
        }
    } catch (const std::exception& error) {
        core::Log::error("The app cannot draw into the window of the activity. {}", error.what());
        quit();
        finishFrame();
        return;
    }
    Player::handleEvent({.type = Event::Type::Resized});
}

void AndroidRuntime::closeWindow() {
    graphics.detachWindow(Player::getGpuDevice());
    window.close();
    pacer.setWindow(false);
}

void AndroidRuntime::requestFrame() {
    if (frameRequested || ended || !pacer.isDrawing()) {
        return;
    }
    postFrameCallback(choreographer, &onFrameCallback, this);
    frameRequested = true;
}

// The app starts with the first frame, when the activity first shows its window.
void AndroidRuntime::drawFrame(double now) {
    if (ended) {
        return;
    }
    if (!started) {
        started = true;
        pacer.setStarted(true);
        Player::initialize();
    }
    replaceLostDevice();
    Player::frame(pacer.takeElapsed(now));
    finishFrame();
}

// Runs the tick of a paused app once it is due, whose suspended app draws nothing, and returns how long the loop may wait for messages.
int AndroidRuntime::tick() {
    const double now = getTime();
    if (pacer.takeTick(now)) {
        replaceLostDevice();
        Player::frame(pacer.takeElapsed(now));
        finishFrame();
    }
    return pacer.getWaitMilliseconds(getTime());
}

// A device that stopped working, after a reset of the GPU, a lost context or `VK_ERROR_DEVICE_LOST`, gives way to a new one, made through the order of graphics APIs again, on which the app starts again.
void AndroidRuntime::replaceLostDevice() {
    const gpu::Device* device = Player::getGpuDevice();
    if (device == nullptr || !graphics.isLost(*device)) {
        return;
    }
    core::Log::warning("The GPU device of \"{}\" stopped working, so the app starts again on a new one.", gpu::DeviceFactory::getApiName(device->getApi()));
    graphics.loseDevice(device->getApi());
    Player::restart();
}

// A quit ends the app and finishes the activity, whose destruction then ends the frame thread. The process lives on, since an exit would run the destructors of static objects while the threads of Android still use them.
void AndroidRuntime::finishFrame() {
    if (quitRequested) {
        quitRequested = false;
        quitOrdered = true;
        Player::handleEvent({.type = Event::Type::QuitRequested});
    }
    if (!quitOrdered || ended) {
        return;
    }
    end();
    GameActivity_finish(&activity);
}

void AndroidRuntime::end() {
    if (ended) {
        return;
    }
    ended = true;
    pacer.setStarted(false);
    Player::cleanup();
}

double AndroidRuntime::getTime() noexcept {
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_nsec) / 1.0e9;
}

} // namespace haylen::platform

extern "C" {

// GameActivity starts the native side of each activity here, and the Java side keeps the state that outlives the process, so the native side ignores the saved state.
JNIEXPORT void GameActivity_onCreate(GameActivity* activity, void*, size_t) {
    haylen::platform::AndroidRuntime::create(*activity);
}
}
