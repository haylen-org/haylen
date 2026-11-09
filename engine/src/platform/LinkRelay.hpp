#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace haylen::platform {

// Carries the links that open the app, such as one of a URL scheme that its plugins declare, from the platform to the engine, which publishes each one as `linkOpened`. Links belong to the process, so the link that launched the app, or one that arrives while no app runs, waits for the next app. Platforms report links from any thread.
class LinkRelay final {
  public:
    static void receive(std::string url);

    // Hands over the links that arrived since the last call, in order, which the engine does on the frame thread.
    [[nodiscard]] static std::vector<std::string> take();

  private:
    static std::mutex& mutex;
    static std::vector<std::string>& links;
};

} // namespace haylen::platform
