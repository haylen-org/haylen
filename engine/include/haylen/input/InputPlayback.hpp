#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "haylen/input/InputRecording.hpp"

namespace haylen::input {

// Hands the steps of a recording back one at a time, in place of the devices, for replays, automated playtests and the reproduction of bug reports. The step counts the steps already played, so a new playback stands before the first step and reads its masks.
class InputPlayback final {
  public:
    explicit InputPlayback(std::shared_ptr<const InputRecording> value);

    [[nodiscard]] const InputRecording& getRecording() const noexcept {
        return *recording;
    }
    [[nodiscard]] std::uint32_t getStep() const noexcept {
        return step;
    }
    [[nodiscard]] bool isFinished() const noexcept {
        return step >= recording->getStepCount();
    }

    // The mask of a player and the hash on the current step, 0 and none once the playback finished.
    [[nodiscard]] std::uint32_t getMask(std::size_t player) const;
    [[nodiscard]] std::optional<std::uint32_t> getHash() const;

    void advance() noexcept;

    // Moves to a step from 0 to the step count. Throws `std::out_of_range` for any other step.
    void seek(std::uint32_t value);

  private:
    std::shared_ptr<const InputRecording> recording;
    std::uint32_t step = 0;
};

} // namespace haylen::input
