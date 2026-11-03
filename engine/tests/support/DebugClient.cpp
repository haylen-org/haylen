#include "support/DebugClient.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace haylen::test {

DebugClient::DebugClient(platform::Host& host) {
    debugger = std::make_unique<debug::Debugger>(host, [this](const core::Json& message) { received.push_back(message); }, [this] { pump(); });
}

int DebugClient::request(const std::string& command, core::Json arguments) {
    ++sequence;
    debugger->receive({{"seq", sequence}, {"type", "request"}, {"command", command}, {"arguments", std::move(arguments)}});
    waiting.push_back(sequence);
    return sequence;
}

void DebugClient::connect(lua_State* L) {
    debugger->attach(L);
    request("initialize", {{"adapterID", "haylen"}});
    request("attach");
    debugger->update(L);
}

void DebugClient::then(Step step) {
    steps.push_back(std::move(step));
}

const core::Json* DebugClient::findResponse(int number) const {
    const auto found = std::ranges::find_if(received, [number](const core::Json& message) { return message.value("type", "") == "response" && message.value("request_seq", 0) == number; });
    return found == received.end() ? nullptr : &*found;
}

const core::Json& DebugClient::getResponse(int number) const {
    const core::Json* response = findResponse(number);
    if (response == nullptr) {
        throw std::runtime_error("The debugger never answered the request " + std::to_string(number) + ".");
    }
    return *response;
}

std::vector<core::Json> DebugClient::getEvents(const std::string& name) const {
    std::vector<core::Json> events;
    for (const core::Json& message : received) {
        if (message.value("type", "") == "event" && message.value("event", "") == name) {
            events.push_back(message.value("body", core::Json::object()));
        }
    }
    return events;
}

// A step that sends no request lets the next one run at the following look.
void DebugClient::pump() {
    std::erase_if(waiting, [this](int number) { return findResponse(number) != nullptr; });
    if (!waiting.empty() || steps.empty()) {
        return;
    }
    Step step = std::move(steps.front());
    steps.pop_front();
    step(*this);
}

} // namespace haylen::test
