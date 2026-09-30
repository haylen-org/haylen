#include "haylen/ai/StateMachine.hpp"

#include <stdexcept>
#include <utility>

namespace haylen::ai {

void StateMachine::add(std::string name, Callbacks callbacks) {
    if (name.empty()) {
        throw std::invalid_argument("A state needs a name.");
    }
    const std::string label = name;
    if (!states.emplace(std::move(name), std::move(callbacks)).second) {
        throw std::invalid_argument("The state machine already has a state named \"" + label + "\".");
    }
}

bool StateMachine::has(std::string_view name) const {
    return states.contains(name);
}

void StateMachine::change(std::string_view name) {
    if (!has(name)) {
        throw std::invalid_argument("The state machine has no state named \"" + std::string(name) + "\".");
    }
    pending.emplace_back(name);
    if (transitioning) {
        return;
    }

    // A callback that throws abandons the queued changes and leaves the machine ready for the next one.
    struct TransitionScope {
        StateMachine& machine;
        ~TransitionScope() {
            machine.pending.clear();
            machine.transitioning = false;
        }
    } scope{*this};
    transitioning = true;

    while (!pending.empty()) {
        std::string next = std::move(pending.front());
        pending.pop_front();

        if (!current.empty()) {
            if (const auto& onExit = states.find(current)->second.exit) {
                onExit();
            }
        }
        previous = std::exchange(current, std::move(next));
        elapsed = 0.0F;
        if (onChange) {
            onChange(previous, current);
        }
        if (const auto& onEnter = states.find(current)->second.enter) {
            onEnter();
        }
    }
}

void StateMachine::update(float deltaSeconds) {
    if (current.empty()) {
        return;
    }
    elapsed += deltaSeconds;
    if (const auto& onUpdate = states.find(current)->second.update) {
        onUpdate(deltaSeconds);
    }
}

} // namespace haylen::ai
