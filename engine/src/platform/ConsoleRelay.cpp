#include "platform/ConsoleRelay.hpp"

#include <utility>

namespace haylen::platform {

std::mutex& ConsoleRelay::mutex = *new std::mutex;
std::vector<ConsoleRelay::PendingLine>& ConsoleRelay::pending = *new std::vector<ConsoleRelay::PendingLine>;
std::vector<ConsoleRelay::PendingListing>& ConsoleRelay::listings = *new std::vector<ConsoleRelay::PendingListing>;
std::vector<ConsoleRelay::Inspection>& ConsoleRelay::inspections = *new std::vector<ConsoleRelay::Inspection>;

void ConsoleRelay::submit(std::string line, Reply reply, const void* owner) {
    const std::scoped_lock lock(mutex);
    pending.push_back({.request = {.line = std::move(line), .reply = std::move(reply)}, .owner = owner});
}

void ConsoleRelay::requestCommands(ListReply reply, const void* owner) {
    const std::scoped_lock lock(mutex);
    listings.push_back({.reply = std::move(reply), .owner = owner});
}

void ConsoleRelay::requestInspection(std::vector<std::string> path, InspectionReply reply) {
    const std::scoped_lock lock(mutex);
    inspections.push_back({.path = std::move(path), .reply = std::move(reply)});
}

std::vector<ConsoleRelay::Inspection> ConsoleRelay::takeInspections() {
    const std::scoped_lock lock(mutex);
    return std::exchange(inspections, {});
}

void ConsoleRelay::withdraw(const void* owner) {
    const std::scoped_lock lock(mutex);
    std::erase_if(pending, [owner](const PendingLine& line) { return line.owner == owner; });
    std::erase_if(listings, [owner](const PendingListing& listing) { return listing.owner == owner; });
}

std::vector<ConsoleRelay::Request> ConsoleRelay::take() {
    std::vector<PendingLine> lines;
    {
        const std::scoped_lock lock(mutex);
        lines.swap(pending);
    }

    std::vector<Request> requests;
    requests.reserve(lines.size());
    for (PendingLine& line : lines) {
        requests.push_back(std::move(line.request));
    }

    return requests;
}

std::vector<ConsoleRelay::ListReply> ConsoleRelay::takeCommandRequests() {
    std::vector<PendingListing> waiting;
    {
        const std::scoped_lock lock(mutex);
        waiting.swap(listings);
    }

    std::vector<ListReply> replies;
    replies.reserve(waiting.size());
    for (PendingListing& listing : waiting) {
        replies.push_back(std::move(listing.reply));
    }

    return replies;
}

} // namespace haylen::platform
