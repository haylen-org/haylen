#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "haylen/debug/Console.hpp"

namespace haylen::platform {

// Carries the lines that arrive from outside the app, from the development server or the web page, to the console of the running engine, and their results back. Lines may arrive from any thread and while no app runs, so they wait in the relay until the debug plugin of the next frame takes them, and each reply runs on the frame thread.
class ConsoleRelay final {
  public:
    using Reply = std::function<void(const debug::Console::Result& result)>;

    struct Request {
        std::string line;
        Reply reply;
    };

    static void submit(std::string line, Reply reply);
    [[nodiscard]] static std::vector<Request> take();

  private:
    static std::mutex& mutex;
    static std::vector<Request>& pending;
};

} // namespace haylen::platform
