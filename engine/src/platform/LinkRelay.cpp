#include "platform/LinkRelay.hpp"

#include <utility>

namespace haylen::platform {

std::mutex& LinkRelay::mutex = *new std::mutex();
std::vector<std::string>& LinkRelay::links = *new std::vector<std::string>();

void LinkRelay::receive(std::string url) {
    const std::scoped_lock lock(mutex);
    links.push_back(std::move(url));
}

bool LinkRelay::hasLinks() {
    const std::scoped_lock lock(mutex);
    return !links.empty();
}

std::vector<std::string> LinkRelay::take() {
    const std::scoped_lock lock(mutex);
    return std::exchange(links, {});
}

} // namespace haylen::platform
