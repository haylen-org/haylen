#include "platform/ConsoleRelay.hpp"

#include <utility>

namespace haylen::platform {

std::mutex& ConsoleRelay::mutex = *new std::mutex;
std::vector<ConsoleRelay::Request>& ConsoleRelay::pending = *new std::vector<ConsoleRelay::Request>;

void ConsoleRelay::submit(std::string line, Reply reply) {
    const std::scoped_lock lock(mutex);
    pending.push_back({.line = std::move(line), .reply = std::move(reply)});
}

std::vector<ConsoleRelay::Request> ConsoleRelay::take() {
    const std::scoped_lock lock(mutex);
    return std::exchange(pending, {});
}

} // namespace haylen::platform
