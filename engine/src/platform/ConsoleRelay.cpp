#include "platform/ConsoleRelay.hpp"

#include <utility>

namespace haylen::platform {

std::mutex& ConsoleRelay::mutex = *new std::mutex;
std::vector<ConsoleRelay::Request>& ConsoleRelay::pending = *new std::vector<ConsoleRelay::Request>;
std::vector<ConsoleRelay::ListReply>& ConsoleRelay::listings = *new std::vector<ConsoleRelay::ListReply>;

void ConsoleRelay::submit(std::string line, Reply reply) {
    const std::scoped_lock lock(mutex);
    pending.push_back({.line = std::move(line), .reply = std::move(reply)});
}

void ConsoleRelay::requestCommands(ListReply reply) {
    const std::scoped_lock lock(mutex);
    listings.push_back(std::move(reply));
}

std::vector<ConsoleRelay::Request> ConsoleRelay::take() {
    const std::scoped_lock lock(mutex);
    return std::exchange(pending, {});
}

std::vector<ConsoleRelay::ListReply> ConsoleRelay::takeCommandRequests() {
    const std::scoped_lock lock(mutex);
    return std::exchange(listings, {});
}

} // namespace haylen::platform
