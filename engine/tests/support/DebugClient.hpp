#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "debug/Debugger.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::platform {
class Host;
}

namespace haylen::test {

// A client of the Debug Adapter Protocol that drives a debugger of its own, the way an editor does through the development server. It records every message the debugger sends, and while the debugger halts the app it takes the steps that the test queued, each one once the responses to the requests of the step before arrived, since a halt handles requests only between its looks at the client.
class DebugClient final {
  public:
    using Step = std::function<void(DebugClient& client)>;

    explicit DebugClient(platform::Host& host);

    [[nodiscard]] debug::Debugger& getDebugger() noexcept {
        return *debugger;
    }

    // Sends a request and returns its sequence number.
    int request(const std::string& command, core::Json arguments = core::Json::object());

    // Lets the debugger follow the Lua state, then initializes and attaches the client, which a halt needs, and handles the requests at once, as a frame does.
    void connect(lua_State* L);

    // Queues a step for the halts to come.
    void then(Step step);

    // The response to the request, or null while it has not arrived.
    [[nodiscard]] const core::Json* findResponse(int number) const;
    [[nodiscard]] const core::Json& getResponse(int number) const;

    // The bodies of the events of the name, in the order they arrived.
    [[nodiscard]] std::vector<core::Json> getEvents(const std::string& name) const;

    [[nodiscard]] bool hasSteps() const noexcept {
        return !steps.empty();
    }

  private:
    void pump();

    std::unique_ptr<debug::Debugger> debugger;
    std::vector<core::Json> received;
    std::deque<Step> steps;
    std::vector<int> waiting;
    int sequence = 0;
};

} // namespace haylen::test
