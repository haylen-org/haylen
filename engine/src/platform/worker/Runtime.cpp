#include "platform/worker/Runtime.hpp"

#include <emscripten/emscripten.h>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>

#include "haylen/core/AppError.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Log.hpp"
#include "platform/CrashGuard.hpp"
#include "platform/Player.hpp"
#include "platform/Services.hpp"
#include "platform/web/WebEntry.hpp"
#include "platform/worker/Host.hpp"

// clang-format off
EM_JS(int, haylen_js_has_animation_frames, (), {
    return typeof requestAnimationFrame === "function" ? 1 : 0;
});

EM_JS(int, haylen_js_gamepads_revision, (), {
    return Module.haylen.gamepads.revision;
});

EM_JS(char*, haylen_js_gamepad_ids, (), {
    return stringToNewUTF8(JSON.stringify(Module.haylen.gamepads.pads.map((pad) => pad.id)));
});

EM_JS(int, haylen_js_read_gamepads, (float* values, int capacity, int recordSize), {
    const pads = Module.haylen.gamepads.pads;
    const count = Math.min(pads.length, capacity);
    for (let pad = 0; pad < count; ++pad) {
        const record = (values >> 2) + pad * recordSize;
        HEAPF32[record] = pads[pad].index;
        HEAPF32.set(pads[pad].buttons, record + 1);
        HEAPF32.set(pads[pad].axes, record + 1 + pads[pad].buttons.length);
    }
    return count;
});
// clang-format on

namespace haylen::platform::worker {

// The worker needs the animation frames of the browser, which every browser that runs WebGPU or WebGL2 in workers gives them, so a browser without them gets the reason instead of frames that a timer paces on its own.
int Runtime::run(int argc, char* argv[]) {
    CrashGuard::install();

    if (!hasAnimationFrames()) {
        const std::string reason = "This browser runs no animation frames in workers, which the Worker mode of the runtime needs. Run the runtime on the page instead.";
        core::Log::error("{}", reason);
        Services::reportError(core::AppError(reason).toJson());
        return 1;
    }

    auto* runtime = new Runtime();
    return runtime->initialize(argc, argv);
}

std::unique_ptr<gpu::Device> Runtime::createDevice() {
    if (openedDevice != nullptr) {
        return std::move(openedDevice);
    }

    if (recovering) {
        recovering = false;
        window.renewGlContext();
    }

    return gpu::DeviceFactory::create(window.getApi(), window.getSurface(), getDeviceOptions());
}

bool Runtime::prepareDevice() {
    return openedDevice != nullptr || gpu::DeviceFactory::prepare(window.getApi(), getDeviceOptions());
}

void Runtime::setFrameRate(int framesPerSecond) {
    frameRate.setTarget(framesPerSecond);
    updateTiming();
}

void Runtime::setMaxPixelDensity(float cap) {
    window.setMaxPixelDensity(cap);
    followDrawable();
}

void Runtime::receiveKey(std::string_view code, bool down, bool repeat, int modifiers) {
    if (const std::optional<Event> key = Events::translateKey(code, down, repeat, modifiers)) {
        deliver(*key);
    }
}

void Runtime::receiveMouse(Events::Mouse change, int button, math::Vec2 position, math::Vec2 movement, int modifiers) {
    if (const std::optional<Event> mouse = Events::translateMouse(change, button, position, movement, modifiers, window.getPixelDensity())) {
        deliver(*mouse);
    }
}

void Runtime::receiveTouch(std::uint64_t id, Touches::Phase phase, math::Vec2 position, int modifiers) {
    if (const std::optional<Event> touch = events.translateTouch(id, phase, position, modifiers, window.getPixelDensity())) {
        deliver(*touch);
    }
}

void Runtime::receiveScroll(math::Vec2 value, bool precise, int modifiers) {
    deliver(Events::translateScroll(value, precise, modifiers));
}

// The canvas box changed size or density, so the swapchain follows the drawable before the app hears it.
void Runtime::receiveChange(Change change) {
    switch (change) {
    case Change::FocusGained:
        deliver({.type = Event::Type::FocusGained});
        return;
    case Change::FocusLost:
        deliver({.type = Event::Type::FocusLost});
        return;
    case Change::MouseEnter:
        deliver({.type = Event::Type::MouseEnter});
        return;
    case Change::MouseLeave:
        deliver({.type = Event::Type::MouseLeave});
        return;
    case Change::Resized:
        followDrawable();
        deliver({.type = Event::Type::Resized});
        return;
    }
}

void Runtime::setPageVisible(bool visible) {
    hidden = !visible;
    updateTiming();
}

void Runtime::onFrame(void* state) {
    auto* runtime = static_cast<Runtime*>(state);
    if (!runtime->closing) {
        runtime->frame();
        return;
    }

    runtime->quit();
    delete runtime;
}

bool Runtime::hasAnimationFrames() noexcept {
    return haylen_js_has_animation_frames() != 0;
}

// Debug builds validate every call of the graphics API, as on the page.
gpu::DeviceOptions Runtime::getDeviceOptions() noexcept {
#if defined(NDEBUG)
    constexpr bool validation = false;
#else
    constexpr bool validation = true;
#endif
    return {.validation = validation, .lowPower = Player::getOpeningConfig().window.lowPowerGpu};
}

// Without a player there is no window and no error screen, so a failure to create one goes to the log and the page, which would otherwise wait for an app that never starts.
int Runtime::initialize(int argc, char* argv[]) {
    running = this;
    try {
        Player::create(argc, argv, std::make_unique<Host>(*this));
    } catch (const std::exception& error) {
        core::Log::error("The player could not start: {}", error.what());
        Services::reportError(core::AppError(error.what()).toJson());
        running = nullptr;
        delete this;
        return 1;
    }

    window.setMaxPixelDensity(Player::getOpeningConfig().window.maxPixelDensity);
    platform::NativeWindow::install(&window);
    apis = Player::getGraphicsApis();
    emscripten_set_main_loop_arg(&Runtime::onFrame, this, 0, false);
    return 0;
}

void Runtime::frame() {
    if (!started) {
        start();
        return;
    }

    const double milliseconds = emscripten_get_now();
    if (!frameRate.takeFrame(milliseconds / 1000.0)) {
        return;
    }

    readGamepads();
    recover();
    Player::frame(timer.next(static_cast<std::uint64_t>(milliseconds * 1.0e6)));
}

// The app starts once its window opened, which takes some frames while WebGPU prepares, since the browser hands its adapter over in a later turn of the event loop of the worker.
void Runtime::start() {
    try {
        if (!openWindow()) {
            return;
        }
    } catch (const std::exception& error) {
        core::Log::error("{}", error.what());
        Services::reportError(core::AppError(error.what()).toJson());
        closing = true;
        return;
    }

    Player::initialize();
    started = true;
}

// Each graphics API in the order of the app prepares the canvas and starts its device, and the first that starts keeps the canvas. The reasons of the ones that failed reach the log. A canvas without area, such as one that the page hides, holds the start until it shows, so no device starts on a drawable without pixels.
bool Runtime::openWindow() {
    for (; nextApi < apis.size(); ++nextApi) {
        const gpu::Device::Api api = apis[nextApi];
        if (!gpu::DeviceFactory::prepare(api, getDeviceOptions())) {
            return false;
        }

        try {
            if (window.getApi() != api) {
                window.open(Player::getOpeningConfig().window, api);
            }

            window.refresh();
            if (!window.getSurfaceSize().hasArea()) {
                return false;
            }

            openedDevice = gpu::DeviceFactory::create(api, window.getSurface(), getDeviceOptions());
            core::Log::info("The app draws through \"{}\".", gpu::DeviceFactory::getApiName(api));
            return true;
        } catch (const std::exception& error) {
            core::Log::warning("The graphics API \"{}\" could not start. {}", gpu::DeviceFactory::getApiName(api), error.what());
            window.close();
        }
    }

    throw std::runtime_error("No graphics API could start in this browser, so the app cannot draw into its canvas.");
}

// A device that stopped working, such as a WebGL2 context that the browser lost, gives way to a new one, which the app starts again on. A device gets one replacement, and a replacement that stops working too ends the app with the reason.
void Runtime::recover() {
    const gpu::Device* device = Player::getGpuDevice();
    if (device == nullptr || !device->isLost()) {
        return;
    }

    if (const std::optional<std::string> failure = deviceRecovery.recordLoss(window.getApi())) {
        Player::fail(*failure);
        return;
    }

    recovering = true;
    Player::restart();
}

// The events before the app started find no app to hear them.
void Runtime::deliver(const Event& event) {
    if (started && !closing) {
        Player::handleEvent(event);
    }
}

// A canvas without area, such as one that its page hides, leaves the device with the last surface that had one, since the engine draws nothing until the canvas has an area again.
void Runtime::followDrawable() {
    window.refresh();
    const gpu::Surface::Size size = window.getSurfaceSize();
    gpu::Device* device = Player::getGpuDevice();
    if (device != nullptr && size.hasArea()) {
        device->resizeSurface(size);
    }
}

// The ids of the controllers change only when the page connected or lost one, so a frame reads them only then and otherwise only the values of their buttons and axes.
void Runtime::readGamepads() {
    const int count = haylen_js_read_gamepads(gamepadValues.data(), static_cast<int>(kMaxControllers), static_cast<int>(kRecordSize));
    const int revision = haylen_js_gamepads_revision();
    if (revision != gamepadRevision) {
        gamepadRevision = revision;
        char* text = haylen_js_gamepad_ids();
        gamepadIds = core::Json::parse(text).get<std::vector<std::string>>();
        std::free(text);
    }

    gamepadRecords.resize(static_cast<std::size_t>(count));
    for (std::size_t pad = 0; pad < gamepadRecords.size(); ++pad) {
        const float* record = gamepadValues.data() + pad * kRecordSize;
        Gamepads::Record& target = gamepadRecords[pad];
        target.index = static_cast<std::size_t>(record[0]);
        target.id = gamepadIds.at(pad);
        std::copy_n(record + 1, Gamepads::kButtonCount, target.buttons.begin());
        std::copy_n(record + 1 + Gamepads::kButtonCount, Gamepads::kAxisCount, target.axes.begin());
    }

    gamepads.update(gamepadRecords);
}

// The animation frames of the worker pace the frames while the page shows, which the frame rate of the app thins out, and timers pace them while it hides.
void Runtime::updateTiming() {
    if (!hidden) {
        emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);
        return;
    }

    const int target = frameRate.getTarget();
    const int rate = target > 0 ? std::min(target, kHiddenRate) : kHiddenRate;
    emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, 1000 / rate);
}

void Runtime::quit() noexcept {
    emscripten_cancel_main_loop();

    if (started) {
        started = false;
        Player::cleanup();
    }

    platform::NativeWindow::install(nullptr);
    openedDevice.reset();
    window.close();
    running = nullptr;
}

} // namespace haylen::platform::worker

extern "C" {

// The input that the page reads over the canvas: a key by its `code`, a change of the mouse with the button that changed, a change of a finger, as began, moved, ended and cancelled, and a scroll, with positions and movements in CSS pixels of the canvas box and the modifiers as the bits of shift, control, alt and the super key.
EMSCRIPTEN_KEEPALIVE void haylen_worker_key(const char* code, int down, int repeat, int modifiers) {
    // clang-format off
    haylen::platform::WebEntry::guard("haylen_worker_key", [&] {
        if (haylen::platform::worker::Runtime* runtime = haylen::platform::worker::Runtime::find()) {
            runtime->receiveKey(code, down != 0, repeat != 0, modifiers);
        }
    });
    // clang-format on
}

EMSCRIPTEN_KEEPALIVE void haylen_worker_mouse(int change, int button, float x, float y, float movementX, float movementY, int modifiers) {
    // clang-format off
    haylen::platform::WebEntry::guard("haylen_worker_mouse", [&] {
        if (haylen::platform::worker::Runtime* runtime = haylen::platform::worker::Runtime::find()) {
            runtime->receiveMouse(static_cast<haylen::platform::worker::Events::Mouse>(change), button, {x, y}, {movementX, movementY}, modifiers);
        }
    });
    // clang-format on
}

EMSCRIPTEN_KEEPALIVE void haylen_worker_touch(double id, int phase, float x, float y, int modifiers) {
    // clang-format off
    haylen::platform::WebEntry::guard("haylen_worker_touch", [&] {
        if (haylen::platform::worker::Runtime* runtime = haylen::platform::worker::Runtime::find()) {
            runtime->receiveTouch(static_cast<std::uint64_t>(id), static_cast<haylen::platform::Touches::Phase>(phase), {x, y}, modifiers);
        }
    });
    // clang-format on
}

EMSCRIPTEN_KEEPALIVE void haylen_worker_scroll(float x, float y, int precise, int modifiers) {
    // clang-format off
    haylen::platform::WebEntry::guard("haylen_worker_scroll", [&] {
        if (haylen::platform::worker::Runtime* runtime = haylen::platform::worker::Runtime::find()) {
            runtime->receiveScroll({x, y}, precise != 0, modifiers);
        }
    });
    // clang-format on
}

// The changes of the window are those of `Runtime::Change`, in its order.
EMSCRIPTEN_KEEPALIVE void haylen_worker_change(int change) {
    // clang-format off
    haylen::platform::WebEntry::guard("haylen_worker_change", [&] {
        if (haylen::platform::worker::Runtime* runtime = haylen::platform::worker::Runtime::find()) {
            runtime->receiveChange(static_cast<haylen::platform::worker::Runtime::Change>(change));
        }
    });
    // clang-format on
}

EMSCRIPTEN_KEEPALIVE void haylen_worker_visibility(int visible) {
    // clang-format off
    haylen::platform::WebEntry::guard("haylen_worker_visibility", [&] {
        if (haylen::platform::worker::Runtime* runtime = haylen::platform::worker::Runtime::find()) {
            runtime->setPageVisible(visible != 0);
        }
    });
    // clang-format on
}
}
