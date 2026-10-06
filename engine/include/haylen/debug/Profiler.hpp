#pragma once

#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>

#include "haylen/debug/ProfileSample.hpp"

namespace haylen::debug {

// Measures named CPU scopes on the frame thread. Scopes with the same name under the same parent add up within a frame, and a short history of frame times feeds graphs. A frame time is the work between `beginFrame` and `endFrame`, while the frame rate comes from the time between the starts of the frames, which also holds the wait for the display.
class Profiler final {
  public:
    // Returns the current time in seconds. Without one, the profiler reads the steady clock.
    using Clock = std::function<double()>;

    explicit Profiler(std::size_t historySize = 240, Clock source = {});

    void beginFrame();
    void endFrame();

    void beginScope(std::string_view name);
    void endScope();

    // Opens and closes the scopes of app scripts. Closing refuses a scope that the engine opened, so a script that closes more scopes than it opened never closes the scope it runs in.
    void beginScriptScope(std::string_view name);
    void endScriptScope();

    // Closes every scope open at the depth or deeper, which does nothing when app code already closed them.
    void endScopesTo(std::size_t depth) noexcept;
    [[nodiscard]] std::size_t getDepth() const noexcept {
        return open.size();
    }

    // The scopes of the last finished frame, parents before their children.
    [[nodiscard]] const std::vector<ProfileSample>& getLastFrame() const noexcept {
        return finished;
    }
    [[nodiscard]] double getLastFrameMilliseconds() const noexcept {
        return lastFrameMilliseconds;
    }
    [[nodiscard]] double getAverageFrameMilliseconds() const noexcept;

    // The frames per second over the history, or 0 before the second frame.
    [[nodiscard]] double getFramesPerSecond() const noexcept;

    // Frame times of the history, oldest first.
    [[nodiscard]] std::vector<float> getFrameHistory() const;

  private:
    struct OpenScope {
        int sample = 0;
        double start = 0.0;
        bool script = false;
    };

    [[nodiscard]] static double getSteadySeconds();

    Clock clock;
    std::vector<ProfileSample> current;
    std::vector<ProfileSample> finished;
    std::vector<OpenScope> open;
    std::vector<float> history;
    std::vector<float> intervals;
    std::size_t next = 0;
    std::size_t recorded = 0;
    std::size_t nextInterval = 0;
    std::size_t intervalCount = 0;
    double frameStart = 0.0;
    double lastFrameMilliseconds = 0.0;
    bool begun = false;
    bool inFrame = false;
};

} // namespace haylen::debug
