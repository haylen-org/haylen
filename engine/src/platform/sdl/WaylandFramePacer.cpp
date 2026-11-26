#include "platform/sdl/WaylandFramePacer.hpp"

#include <dlfcn.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <SDL3/SDL_properties.h>

#include <algorithm>
#include <utility>

#include "platform/linux/LinuxGlib.hpp"
#include "platform/sdl/WaylandDisplays.hpp"

namespace haylen::platform::sdl {

// SDL loaded the library of Wayland when it chose the Wayland driver, so the pacer reaches the same library and the same display without loading another copy.
std::unique_ptr<WaylandFramePacer> WaylandFramePacer::start(SDL_Window* window, std::uint32_t wakeType, int hiddenRate) {
    if (!WaylandDisplays::isSession()) {
        return nullptr;
    }

    void* handle = dlopen("libwayland-client.so.0", RTLD_NOW | RTLD_NOLOAD);
    auto* surface = static_cast<wl_proxy*>(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));
    if (handle == nullptr || surface == nullptr) {
        return nullptr;
    }

    Library functions;
    bool complete = true;
    complete = LinuxGlib::find(handle, functions.marshal, "wl_proxy_marshal_flags") && complete;
    complete = LinuxGlib::find(handle, functions.addListener, "wl_proxy_add_listener") && complete;
    complete = LinuxGlib::find(handle, functions.destroy, "wl_proxy_destroy") && complete;
    complete = LinuxGlib::find(handle, functions.getVersion, "wl_proxy_get_version") && complete;
    functions.callbackInterface = static_cast<const wl_interface*>(dlsym(handle, "wl_callback_interface"));
    if (!complete || functions.callbackInterface == nullptr) {
        dlclose(handle);
        return nullptr;
    }

    // The library stays loaded while SDL uses it, so the handle that this lookup took goes back at once.
    dlclose(handle);
    std::unique_ptr<WaylandFramePacer> pacer(new WaylandFramePacer(functions, surface, wakeType));
    pacer->setHiddenRate(hiddenRate);
    return pacer;
}

WaylandFramePacer::WaylandFramePacer(const Library& functions, wl_proxy* windowSurface, std::uint32_t type) : library(functions), surface(windowSurface), wakeType(type) {}

WaylandFramePacer::~WaylandFramePacer() {
    if (timer != 0) {
        SDL_RemoveTimer(timer);
    }

    if (pending != nullptr) {
        library.destroy(pending);
    }
}

void WaylandFramePacer::setHiddenRate(int framesPerSecond) {
    const int rate = std::max(framesPerSecond, 1);
    if (rate == timerRate) {
        return;
    }

    if (timer != 0) {
        SDL_RemoveTimer(timer);
    }

    timerRate = rate;
    const auto interval = static_cast<std::uint64_t>(SDL_NS_PER_SECOND / rate);
    timer = SDL_AddTimerNS(interval, &WaylandFramePacer::tickTimer, this);
}

void WaylandFramePacer::setSuspended(bool value) noexcept {
    gate.setSuspended(value);
}

void WaylandFramePacer::receiveWake(const SDL_UserEvent& event) noexcept {
    if (event.code != kTickCode) {
        return;
    }

    tickPending.store(false, std::memory_order_relaxed);
    if (gate.tick(getSeconds())) {
        frameDue = true;
    }
}

bool WaylandFramePacer::takeFrame() noexcept {
    return std::exchange(frameDue, false);
}

// The callback goes with the next commit of the surface, the one that presents the frame.
bool WaylandFramePacer::beginFrame() {
    if (!gate.takePresent(getSeconds())) {
        return false;
    }

    pending = library.marshal(surface, WL_SURFACE_FRAME, library.callbackInterface, library.getVersion(surface), 0, nullptr);
    static constexpr wl_callback_listener kListener{.done = &WaylandFramePacer::receiveCallback};
    library.addListener(pending, reinterpret_cast<void (**)()>(const_cast<wl_callback_listener*>(&kListener)), this);
    return true;
}

void WaylandFramePacer::endFrame(bool presented) {
    if (!presented) {
        library.marshal(surface, WL_SURFACE_COMMIT, nullptr, library.getVersion(surface), 0);
    }
}

double WaylandFramePacer::getSeconds() noexcept {
    return static_cast<double>(SDL_GetTicksNS()) / static_cast<double>(SDL_NS_PER_SECOND);
}

void WaylandFramePacer::skipFrame() {
    if (beginFrame()) {
        endFrame(false);
    }
}

void WaylandFramePacer::receiveCallback(void* data, wl_callback* callback, std::uint32_t) {
    auto& pacer = *static_cast<WaylandFramePacer*>(data);
    pacer.library.destroy(reinterpret_cast<wl_proxy*>(callback));
    pacer.pending = nullptr;
    pacer.gate.receiveCallback();
    pacer.frameDue = true;
    pacer.push(0);
}

std::uint64_t WaylandFramePacer::tickTimer(void* data, SDL_TimerID, std::uint64_t interval) {
    auto& pacer = *static_cast<WaylandFramePacer*>(data);
    if (!pacer.tickPending.exchange(true, std::memory_order_relaxed)) {
        pacer.push(kTickCode);
    }

    return interval;
}

// An event that SDL cannot queue leaves the wake to the next callback or tick.
void WaylandFramePacer::push(std::int32_t code) noexcept {
    SDL_Event event{};
    event.type = wakeType;
    event.user.code = code;
    if (!SDL_PushEvent(&event) && code == kTickCode) {
        tickPending.store(false, std::memory_order_relaxed);
    }
}

} // namespace haylen::platform::sdl
