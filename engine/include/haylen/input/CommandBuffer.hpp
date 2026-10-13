#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "haylen/core/RingBuffer.hpp"

namespace haylen::input {

class ActionMap;

// Turns the buttons held on each fixed step into one kept command: a single press, or a named sequence of presses typed within a number of steps, chosen by priority when several complete on one step. A press in a later step replaces the kept command, which expires after its lifetime in steps unless the buffer is frozen. Time counts in steps, so the same masks always type the same commands.
class CommandBuffer final {
  public:
    static constexpr std::size_t kMaxButtons = 32;
    static constexpr std::size_t kMaxSteps = 16;

    // Steps of a command that name the direction the facing gives, the forward or the back button of `Options::directions`.
    static constexpr std::size_t kForward = std::numeric_limits<std::size_t>::max();
    static constexpr std::size_t kBack = kForward - 1;

    // A sequence of button presses with nothing pressed between them. A chord is a sequence with `maxTicks` 0, whose buttons go down on the same step, and a command without a lifetime stays until it is consumed or replaced.
    struct Command {
        std::string name;
        std::vector<std::size_t> steps;
        std::optional<std::uint32_t> maxTicks;
        int priority = 0;
        std::optional<std::uint32_t> lifetime;
        bool clearsHistory = true;
    };

    // The buttons are also the order of presses within one step. Every button types a single-press command with its lifetime, and `directions` holds the back and the forward buttons of a character that faces right.
    struct Options {
        std::vector<std::string> buttons;
        std::vector<std::optional<std::uint32_t>> lifetimes;
        std::vector<Command> commands;
        std::optional<std::array<std::size_t, 2>> directions;
    };

    // Throws `std::invalid_argument` for options without buttons or with more than `kMaxButtons`, a lifetime of 0, a command without a name or steps, a step that names no button, a direction step without directions, a sequence longer than `kMaxSteps` and a name used twice.
    explicit CommandBuffer(Options value);

    // Takes the buttons down on this step as bit `n` for button `n`, or reads them from the step state of the actions named like the buttons, which counts a tap shorter than a step as held for one step.
    void update(std::uint32_t downMask) noexcept;
    void update(const ActionMap& map) noexcept;

    // Takes the buttons held now as already down, so a button held across a pause never types a press.
    void settle(std::uint32_t downMask) noexcept;
    void settle(const ActionMap& map) noexcept;

    // Forgets the kept command and the typed presses and keeps the buttons that are held.
    void clear() noexcept;

    void setFrozen(bool value) noexcept {
        frozen = value;
    }
    [[nodiscard]] bool isFrozen() const noexcept {
        return frozen;
    }
    // A negative facing turns the character left, which swaps the forward and the back buttons.
    void setFacing(int value) noexcept {
        facing = value < 0 ? -1 : 1;
    }
    [[nodiscard]] int getFacing() const noexcept {
        return facing;
    }
    [[nodiscard]] std::uint32_t getTick() const noexcept {
        return tick;
    }

    // Commands count the single presses of the buttons first, in button order, and the named commands after them.
    [[nodiscard]] std::size_t getCommandCount() const noexcept {
        return options.buttons.size() + options.commands.size();
    }
    [[nodiscard]] const std::string& getCommandName(std::size_t command) const;
    [[nodiscard]] std::optional<std::size_t> findCommand(std::string_view name) const noexcept;
    [[nodiscard]] std::optional<std::size_t> findButton(std::string_view name) const noexcept;
    [[nodiscard]] const Options& getOptions() const noexcept {
        return options;
    }

    [[nodiscard]] std::optional<std::size_t> peek() const noexcept;
    bool consume(std::size_t command) noexcept;
    [[nodiscard]] bool isHeld(std::size_t button) const noexcept;

  private:
    struct Press {
        std::size_t button = 0;
        std::uint32_t tick = 0;
    };

    struct Kept {
        std::size_t command = 0;
        std::uint32_t age = 0;
    };

    void validate() const;
    [[nodiscard]] std::uint32_t readMask(const ActionMap& map) const noexcept;
    [[nodiscard]] bool completes(const Command& command) const noexcept;
    [[nodiscard]] std::size_t resolveStep(std::size_t step) const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> getLifetime(std::size_t command) const noexcept;
    [[nodiscard]] int getPriority(std::size_t command) const noexcept;
    void age() noexcept;

    Options options;
    core::RingBuffer<Press> history{kMaxSteps};
    std::optional<Kept> kept;
    std::uint32_t held = 0;
    std::uint32_t tick = 0;
    int facing = 1;
    bool frozen = false;
};

} // namespace haylen::input
