#pragma once

#include <android/native_window.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "haylen/gpu/Surface.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Output.hpp"
#include "platform/Host.hpp"
#include "platform/android/AndroidGraphics.hpp"

namespace haylen::platform {

// The outputs of the app on the other displays of the device, which `HaylenOutputs` shows in presentations of the display manager. The UI thread hands over the window of each presentation as it comes, changes and goes, and the frame thread reads it into the report of the output, with another drawable of the EGL context under OpenGL ES. A window that goes waits on the UI thread, within a bound, until the frame that read its end lets go of it, since Android destroys it once the callback returns.
class AndroidOutputs final {
  public:
    // How long the UI thread waits for the frame thread to let go of a window that goes.
    static constexpr std::chrono::seconds kReleaseTimeout{1};

    // Open and close the presentation of an output, from the frame thread.
    static void open(std::uint32_t number, std::string_view display, const Output::Options& options);
    static void close(std::uint32_t number);

    // The report of an output on the frame thread, which makes the drawable of a new window for the graphics of the activity.
    [[nodiscard]] static Host::OutputReport getReport(std::uint32_t number, const AndroidGraphics& graphics);

    // Mark the frames of the frame thread, so the windows that went before a frame began are let go of once the frame read their end.
    static void beginFrame() noexcept;
    static void endFrame() noexcept;

    // Takes the window of an output, which `window` holds a reference to, or its end when `window` is null, from the UI thread.
    static void receiveSurface(std::uint32_t number, ANativeWindow* window, int width, int height, float scale);
    static void receiveEnding(std::uint32_t number, Output::Ending ending, std::string reason);
    static void receiveTouch(std::uint32_t number, Event::Type type, std::span<const int> ids, std::span<const float> points, int changed);

  private:
    struct Entry {
        ANativeWindow* window = nullptr;
        int width = 0;
        int height = 0;
        float scale = 1.0F;
        std::unique_ptr<gpu::GlContext> drawable;
        std::optional<Output::Ending> ending;
        std::string reason;
    };

    // A window that went, which the frame thread lets go of once a frame that began after it ended.
    struct Retired {
        std::uint64_t sequence = 0;
        ANativeWindow* window = nullptr;
        std::unique_ptr<gpu::GlContext> drawable;
    };

    static void retire(Entry& entry);
    static void release(Retired& retired) noexcept;

    static std::mutex& mutex;
    static std::condition_variable& released;
    static std::unordered_map<std::uint32_t, Entry>& entries;
    static std::vector<Retired>& retiring;
    static std::uint64_t retiredSequence;
    static std::uint64_t frameSequence;
    static std::uint64_t releasedSequence;
};

} // namespace haylen::platform
