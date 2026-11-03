#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "haylen/platform/Event.hpp"
#include "haylen/platform/Output.hpp"
#include "platform/Host.hpp"

namespace haylen::platform {

// The outputs of the app on the other screens of a browser, which `platform/web/haylen-runtime.js` shows in windows that it opens on their screens, each with a canvas that the WebGPU device presents into. The page reports the canvas as it comes and changes size, the end of its window and the pointer over it.
class WebOutputs final {
  public:
    // Opens the window of an output for a canvas of the sample count, which only the WebGPU device draws into, since a WebGL2 context belongs to its one canvas.
    static void open(std::uint32_t number, std::string_view display, const Output::Options& options, int sampleCount);
    static void close(std::uint32_t number);
    [[nodiscard]] static Host::OutputReport getReport(std::uint32_t number);

    // What the page reports of the window of an output.
    static void receiveCanvas(std::uint32_t number, int width, int height, float scale);
    static void receiveEnding(std::uint32_t number, Output::Ending ending, std::string reason);
    static void receivePointer(std::uint32_t number, Event event);

  private:
    struct Entry {
        std::string canvas;
        int sampleCount = 1;
        Host::OutputReport report;
    };

    static std::unordered_map<std::uint32_t, Entry>& entries;
};

} // namespace haylen::platform
