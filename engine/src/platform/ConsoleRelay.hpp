#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/debug/Console.hpp"

namespace haylen::platform {

// Carries what arrives from outside the app for the console of the running engine, from the development server or the web page: lines to run and requests for the list of commands, and their answers back. Requests may arrive from any thread and while no app runs, so they wait in the relay until the debug plugin of the next frame takes them, and each answer runs on the frame thread.
class ConsoleRelay final {
  public:
    using Reply = std::function<void(const debug::Console::Result& result)>;

    // Receives the commands as an array of `{name, usage, help, arguments}`, where `arguments` names the kind of each argument, with `?` after an optional one.
    using ListReply = std::function<void(const core::Json& commands)>;

    struct Request {
        std::string line;
        Reply reply;
    };

    static void submit(std::string line, Reply reply);
    static void requestCommands(ListReply reply);
    [[nodiscard]] static std::vector<Request> take();
    [[nodiscard]] static std::vector<ListReply> takeCommandRequests();

  private:
    static std::mutex& mutex;
    static std::vector<Request>& pending;
    static std::vector<ListReply>& listings;
};

} // namespace haylen::platform
