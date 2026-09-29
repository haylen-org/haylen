#include "haylen/debug/Monitor.hpp"

#include <stdexcept>
#include <utility>

namespace haylen::debug {

Monitor::Monitor(std::string monitorName, Sampler source, std::size_t historySize) : name(std::move(monitorName)), sampler(std::move(source)), history(historySize) {
    if (name.empty() || !sampler) {
        throw std::invalid_argument("A monitor needs a name and a function that samples its value.");
    }
}

void Monitor::sample() {
    const std::optional<double> sampled = sampler();
    if (!sampled) {
        return;
    }
    value = *sampled;
    history.push(static_cast<float>(value));
}

std::vector<float> Monitor::getHistory() const {
    return {history.begin(), history.end()};
}

} // namespace haylen::debug
