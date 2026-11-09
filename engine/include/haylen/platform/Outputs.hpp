#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "haylen/gpu/Surface.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Output.hpp"

namespace haylen::core {
class EventBus;
}

namespace haylen::graphics {
class Device;
}

namespace haylen::platform {

class Displays;
class Host;

// The outputs of the app on other displays, as `Output` describes. The engine owns it and opens the surface of each output on the GPU device as the platform reports it, and it publishes `outputShown`, `outputHidden`, `outputResized` and `outputClosed`, and the pointer over an output as `outputInput`. Every method runs on the frame thread.
class Outputs final {
  public:
    Outputs(Host& host, Displays& displays, graphics::Device& graphics, core::EventBus& events);
    ~Outputs();

    Outputs(const Outputs&) = delete;
    Outputs& operator=(const Outputs&) = delete;

    // Opens an output on the display with the id. Throws `std::invalid_argument` for a display that is not connected, the display of the window or a display that shows another output.
    [[nodiscard]] std::shared_ptr<Output> open(std::string_view display, const Output::Options& options);

    // Closes an output that is not closed yet, which publishes `outputClosed` with the ending `app`.
    void close(Output& output);

    // The outputs that are not closed, in the order they opened.
    [[nodiscard]] std::span<const std::shared_ptr<Output>> getAll() const noexcept;

    // Reads the report of the output with the number again, as the platform announced with `Event::Type::OutputChanged`.
    void refresh(std::uint32_t number);

    // Ends the outputs whose display the displays no longer list, which publishes `outputClosed` with the ending `disconnected`.
    void followDisplays();

    // Publishes an event of the pointer over an output.
    void receiveInput(const Event& event);

  private:
    [[nodiscard]] Output* find(std::uint32_t number) const noexcept;
    void show(Output& output, const gpu::Surface& surface, float scale);
    void hide(Output& output);
    void end(Output& output, Output::Ending ending, std::string_view reason);
    void releaseSurface(Output& output);

    Host& host;
    Displays& displays;
    graphics::Device& graphics;
    core::EventBus& events;
    std::vector<std::shared_ptr<Output>> outputs;
    std::vector<std::shared_ptr<Output>> snapshot;
    std::uint32_t nextId = 1;
};

} // namespace haylen::platform
