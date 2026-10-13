#include "haylen/input/CommandBuffer.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/input/ActionMap.hpp"

namespace haylen::input {

CommandBuffer::CommandBuffer(Options value) : options(std::move(value)) {
    validate();
}

void CommandBuffer::validate() const {
    const std::size_t buttonCount = options.buttons.size();
    if (buttonCount == 0 || buttonCount > kMaxButtons) {
        throw std::invalid_argument("A command buffer needs from 1 to " + std::to_string(kMaxButtons) + " buttons.");
    }
    if (options.lifetimes.size() != buttonCount) {
        throw std::invalid_argument("A command buffer needs one lifetime for each button.");
    }
    if (options.directions && (options.directions->at(0) >= buttonCount || options.directions->at(1) >= buttonCount)) {
        throw std::invalid_argument("The directions of a command buffer must name two of its buttons.");
    }

    std::vector<std::string_view> names(options.buttons.begin(), options.buttons.end());
    if (std::ranges::any_of(options.lifetimes, [](const std::optional<std::uint32_t>& lifetime) { return lifetime == 0U; })) {
        throw std::invalid_argument("A command lasts at least one step.");
    }
    for (const Command& command : options.commands) {
        if (command.name.empty() || command.steps.empty()) {
            throw std::invalid_argument("A command needs a name and at least one step.");
        }
        if (command.steps.size() > kMaxSteps) {
            throw std::invalid_argument("The command \"" + command.name + "\" has more than " + std::to_string(kMaxSteps) + " steps.");
        }
        for (const std::size_t step : command.steps) {
            const bool direction = step == kForward || step == kBack;
            if (direction && !options.directions) {
                throw std::invalid_argument("The command \"" + command.name + "\" names a direction, which needs the directions of the buffer.");
            }
            if (!direction && step >= buttonCount) {
                throw std::invalid_argument("The command \"" + command.name + "\" names a button that the buffer does not have.");
            }
        }
        if (command.lifetime == 0U) {
            throw std::invalid_argument("A command lasts at least one step.");
        }
        names.push_back(command.name);
    }

    std::ranges::sort(names);
    if (const auto repeated = std::ranges::adjacent_find(names); repeated != names.end()) {
        throw std::invalid_argument("The command name \"" + std::string(*repeated) + "\" is used by more than one command.");
    }
}

const std::string& CommandBuffer::getCommandName(std::size_t command) const {
    const std::size_t buttonCount = options.buttons.size();
    return command < buttonCount ? options.buttons.at(command) : options.commands.at(command - buttonCount).name;
}

std::optional<std::size_t> CommandBuffer::findCommand(std::string_view name) const noexcept {
    if (const std::optional<std::size_t> button = findButton(name)) {
        return button;
    }
    const auto found = std::ranges::find(options.commands, name, &Command::name);
    return found == options.commands.end() ? std::nullopt : std::optional(options.buttons.size() + static_cast<std::size_t>(found - options.commands.begin()));
}

std::optional<std::size_t> CommandBuffer::findButton(std::string_view name) const noexcept {
    const auto found = std::ranges::find(options.buttons, name);
    return found == options.buttons.end() ? std::nullopt : std::optional(static_cast<std::size_t>(found - options.buttons.begin()));
}

std::optional<std::uint32_t> CommandBuffer::getLifetime(std::size_t command) const noexcept {
    const std::size_t buttonCount = options.buttons.size();
    return command < buttonCount ? options.lifetimes[command] : options.commands[command - buttonCount].lifetime;
}

int CommandBuffer::getPriority(std::size_t command) const noexcept {
    const std::size_t buttonCount = options.buttons.size();
    return command < buttonCount ? 0 : options.commands[command - buttonCount].priority;
}

std::size_t CommandBuffer::resolveStep(std::size_t step) const noexcept {
    if (step != kForward && step != kBack) {
        return step;
    }
    const bool forward = (step == kForward) == (facing > 0);
    return forward ? options.directions->at(1) : options.directions->at(0);
}

// A command completes when the typed presses end with its steps, the first and the last of them within its ticks.
bool CommandBuffer::completes(const Command& command) const noexcept {
    const std::size_t length = command.steps.size();
    if (history.size() < length) {
        return false;
    }
    const std::size_t first = history.size() - length;
    for (std::size_t index = 0; index < length; ++index) {
        if (history[first + index].button != resolveStep(command.steps[index])) {
            return false;
        }
    }
    return !command.maxTicks || history.back().tick - history[first].tick <= *command.maxTicks;
}

// A lifetime of one step ends with the step it was typed on, and longer lifetimes stop counting while the buffer is frozen.
void CommandBuffer::age() noexcept {
    if (!kept) {
        return;
    }
    const std::optional<std::uint32_t> lifetime = getLifetime(kept->command);
    if (!lifetime) {
        return;
    }
    if (*lifetime == 1 || !frozen) {
        ++kept->age;
    }
    if (kept->age >= *lifetime) {
        kept.reset();
    }
}

void CommandBuffer::update(std::uint32_t downMask) noexcept {
    ++tick;
    age();
    const std::uint32_t pressed = downMask & ~held;
    held = downMask;
    if (pressed == 0) {
        return;
    }

    // Every press offers its single press and every command it completes, and the highest priority of the step wins, the later offer on a tie.
    std::optional<std::size_t> best;
    // clang-format off
    const auto offer = [this, &best](std::size_t command) {
        if (!best || getPriority(command) >= getPriority(*best)) {
            best = command;
        }
    };
    // clang-format on
    const std::size_t buttonCount = options.buttons.size();
    for (std::size_t button = 0; button < buttonCount; ++button) {
        if ((pressed & (1U << button)) == 0) {
            continue;
        }
        history.push({.button = button, .tick = tick});
        offer(button);

        bool clearHistory = false;
        for (std::size_t index = 0; index < options.commands.size(); ++index) {
            const Command& command = options.commands[index];
            if (completes(command)) {
                offer(buttonCount + index);
                clearHistory = clearHistory || command.clearsHistory;
            }
        }
        if (clearHistory) {
            history.clear();
        }
    }
    kept = Kept{.command = *best};
}

void CommandBuffer::update(const ActionMap& map) noexcept {
    update(readMask(map));
}

void CommandBuffer::settle(std::uint32_t downMask) noexcept {
    held = downMask;
}

void CommandBuffer::settle(const ActionMap& map) noexcept {
    settle(readMask(map));
}

std::uint32_t CommandBuffer::readMask(const ActionMap& map) const noexcept {
    std::uint32_t mask = 0;
    for (std::size_t button = 0; button < options.buttons.size(); ++button) {
        if (map.isStepDown(options.buttons[button])) {
            mask |= 1U << button;
        }
    }
    return mask;
}

void CommandBuffer::clear() noexcept {
    history.clear();
    kept.reset();
}

std::optional<std::size_t> CommandBuffer::peek() const noexcept {
    return kept ? std::optional(kept->command) : std::nullopt;
}

bool CommandBuffer::consume(std::size_t command) noexcept {
    if (!kept || kept->command != command) {
        return false;
    }
    kept.reset();
    return true;
}

bool CommandBuffer::isHeld(std::size_t button) const noexcept {
    return button < options.buttons.size() && (held & (1U << button)) != 0;
}

} // namespace haylen::input
