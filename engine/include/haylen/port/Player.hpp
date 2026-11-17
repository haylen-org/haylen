#pragma once

#include <memory>
#include <vector>

#include "haylen/gpu/Device.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/port/Host.hpp"
#include "haylen/port/Services.hpp"

namespace haylen::port {

// Plays apps for the runtime of a port, the way the player plays them on every platform: it reads the command line, plays the package it names or the bundled one, replaces the running app whenever a package restarts it and hands the app the events of the platform. The runtime installs the services of its platform once, creates the player with its window, opens the window for the graphics APIs of `getGraphicsApis` until a device starts, calls `initialize`, then `frame` once a frame and `cleanup` when the window closes.
class Player final {
  public:
    // Installs the services of the platform for the rest of the process, before the first player. Throws `std::invalid_argument` when the platform or the displays are missing or the services were installed already.
    static void installServices(Services services);

    // Creates the player of the process for the command line, with the window of the port.
    static void create(int argc, char* argv[], std::unique_ptr<Host> host);

    // The graphics APIs the window tries in turn until one starts: the one that `--graphics-api` names, or else the ones of `window.graphicsApis` of the app, then the other APIs of the build.
    [[nodiscard]] static const std::vector<gpu::Device::Api>& getGraphicsApis() noexcept;

    // Starts the app once the window is open, plays one frame of the seconds since the last one, and ends the app as the window closes.
    static void initialize();
    static void frame(double seconds);
    static void cleanup();

    // Hands the app an event of the window or the platform on the frame thread, or queues one from any thread for the start of the next frame.
    static void handleEvent(const platform::Event& event);
    static void postEvent(platform::Event event);

    // The GPU device of the running app, which the runtime tells when the surface of its window changes, or null while no app runs.
    [[nodiscard]] static gpu::Device* getGpuDevice() noexcept;

    // The status that the last app quit with, or 1 when the engine could not start, which the process exits with.
    [[nodiscard]] static int getExitStatus() noexcept;
};

} // namespace haylen::port
