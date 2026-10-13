#include "haylen/input/InputPlayback.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace haylen::input {

InputPlayback::InputPlayback(std::shared_ptr<const InputRecording> value) : recording(std::move(value)) {}

std::uint32_t InputPlayback::getMask(std::size_t player) const {
    if (player >= recording->getHeader().players) {
        throw std::out_of_range("The input recording has no player " + std::to_string(player + 1) + ".");
    }
    return isFinished() ? 0 : recording->getMask(step, player);
}

std::optional<std::uint32_t> InputPlayback::getHash() const {
    return recording->getHash(step);
}

void InputPlayback::advance() noexcept {
    if (!isFinished()) {
        ++step;
    }
}

void InputPlayback::seek(std::uint32_t value) {
    if (value > recording->getStepCount()) {
        throw std::out_of_range("The playback has steps from 0 to " + std::to_string(recording->getStepCount()) + ".");
    }
    step = value;
}

} // namespace haylen::input
