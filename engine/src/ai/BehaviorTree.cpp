#include "haylen/ai/BehaviorTree.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace haylen::ai {

const std::array<std::pair<std::string_view, BehaviorTree::Status>, 3> BehaviorTree::kStatusNames{{{"success", Status::Success}, {"failure", Status::Failure}, {"running", Status::Running}}};

BehaviorTree::Node BehaviorTree::sequence(std::vector<Node> children) {
    return {.kind = Kind::Sequence, .children = std::move(children)};
}

BehaviorTree::Node BehaviorTree::selector(std::vector<Node> children) {
    return {.kind = Kind::Selector, .children = std::move(children)};
}

BehaviorTree::Node BehaviorTree::parallel(std::vector<Node> children, int successes) {
    return {.kind = Kind::Parallel, .children = std::move(children), .count = successes};
}

BehaviorTree::Node BehaviorTree::decorate(Kind kind, Node child, int count, float seconds) {
    Node node{.kind = kind, .count = count, .seconds = seconds};
    node.children.push_back(std::move(child));
    return node;
}

BehaviorTree::Node BehaviorTree::inverter(Node child) {
    return decorate(Kind::Inverter, std::move(child), 0, 0.0F);
}

BehaviorTree::Node BehaviorTree::succeeder(Node child) {
    return decorate(Kind::Succeeder, std::move(child), 0, 0.0F);
}

BehaviorTree::Node BehaviorTree::failer(Node child) {
    return decorate(Kind::Failer, std::move(child), 0, 0.0F);
}

BehaviorTree::Node BehaviorTree::repeater(Node child, int count) {
    return decorate(Kind::Repeater, std::move(child), count, 0.0F);
}

BehaviorTree::Node BehaviorTree::retry(Node child, int count) {
    return decorate(Kind::Retry, std::move(child), count, 0.0F);
}

BehaviorTree::Node BehaviorTree::cooldown(Node child, float seconds) {
    return decorate(Kind::Cooldown, std::move(child), 0, seconds);
}

BehaviorTree::Node BehaviorTree::timeout(Node child, float seconds) {
    return decorate(Kind::Timeout, std::move(child), 0, seconds);
}

BehaviorTree::Node BehaviorTree::wait(float seconds) {
    return {.kind = Kind::Wait, .seconds = seconds};
}

BehaviorTree::Node BehaviorTree::condition(ConditionFunction predicate) {
    return {.kind = Kind::Condition, .condition = std::move(predicate)};
}

BehaviorTree::Node BehaviorTree::action(ActionFunction function) {
    return {.kind = Kind::Action, .action = std::move(function)};
}

std::optional<BehaviorTree::Status> BehaviorTree::statusFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kStatusNames, name, &std::pair<std::string_view, Status>::first);
    return found != kStatusNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view BehaviorTree::statusName(Status value) noexcept {
    return std::ranges::find(kStatusNames, value, &std::pair<std::string_view, Status>::second)->first;
}

BehaviorTree::BehaviorTree(Node root) {
    compile(std::move(root));
}

// Nodes are stored depth first, so the root comes first and every child after its parent.
std::uint32_t BehaviorTree::compile(Node node) {
    const bool decorator = node.kind != Kind::Sequence && node.kind != Kind::Selector && node.kind != Kind::Parallel && node.kind != Kind::Wait && node.kind != Kind::Condition && node.kind != Kind::Action;
    if (decorator && node.children.size() != 1) {
        throw std::invalid_argument("A behavior tree decorator needs exactly one child.");
    }
    if ((node.kind == Kind::Condition && !node.condition) || (node.kind == Kind::Action && !node.action)) {
        throw std::invalid_argument("A behavior tree leaf needs a function.");
    }
    if (node.kind == Kind::Parallel && (node.count < 1 || static_cast<std::size_t>(node.count) > node.children.size())) {
        throw std::invalid_argument("A parallel node needs between one success and as many successes as it has children.");
    }

    const auto index = static_cast<std::uint32_t>(nodes.size());
    nodes.push_back({.kind = node.kind, .action = std::move(node.action), .condition = std::move(node.condition), .count = node.count, .seconds = node.seconds});
    std::vector<std::uint32_t> children;
    for (Node& child : node.children) {
        children.push_back(compile(std::move(child)));
    }
    nodes[index].children = std::move(children);
    return index;
}

BehaviorTree::Status BehaviorTree::tick(float deltaSeconds) {
    time += deltaSeconds;
    status = run(0, deltaSeconds);
    return status;
}

void BehaviorTree::reset() {
    for (std::uint32_t index = 0; index < nodes.size(); ++index) {
        resetNode(index);
        nodes[index].readyAt = 0.0F;
    }
    status = Status::Success;
}

void BehaviorTree::resetNode(std::uint32_t index) {
    State& node = nodes[index];
    node.current = 0;
    node.finished = 0;
    node.elapsed = 0.0F;
    node.results.clear();
    for (const std::uint32_t child : node.children) {
        resetNode(child);
    }
}

BehaviorTree::Status BehaviorTree::runParallel(State& node, float deltaSeconds) {
    if (node.results.size() != node.children.size()) {
        node.results.assign(node.children.size(), Status::Running);
    }

    int successes = 0;
    int failures = 0;
    for (std::size_t child = 0; child < node.children.size(); ++child) {
        if (node.results[child] == Status::Running) {
            node.results[child] = run(node.children[child], deltaSeconds);
        }
        successes += node.results[child] == Status::Success ? 1 : 0;
        failures += node.results[child] == Status::Failure ? 1 : 0;
    }

    const bool succeeded = successes >= node.count;
    if (!succeeded && failures <= static_cast<int>(node.children.size()) - node.count) {
        return Status::Running;
    }
    for (const std::uint32_t child : node.children) {
        resetNode(child);
    }
    node.results.clear();
    return succeeded ? Status::Success : Status::Failure;
}

BehaviorTree::Status BehaviorTree::run(std::uint32_t index, float deltaSeconds) {
    State& node = nodes[index];
    switch (node.kind) {
    case Kind::Sequence:
    case Kind::Selector: {
        const Status stop = node.kind == Kind::Sequence ? Status::Failure : Status::Success;
        while (node.current < node.children.size()) {
            const Status result = run(node.children[node.current], deltaSeconds);
            if (result == Status::Running) {
                return result;
            }
            if (result == stop) {
                node.current = 0;
                return stop;
            }
            ++node.current;
        }
        node.current = 0;
        return stop == Status::Failure ? Status::Success : Status::Failure;
    }
    case Kind::Parallel:
        return runParallel(node, deltaSeconds);
    case Kind::Inverter: {
        const Status result = run(node.children.front(), deltaSeconds);
        return result == Status::Running ? result : (result == Status::Success ? Status::Failure : Status::Success);
    }
    case Kind::Succeeder:
    case Kind::Failer: {
        const Status result = run(node.children.front(), deltaSeconds);
        return result == Status::Running ? result : (node.kind == Kind::Succeeder ? Status::Success : Status::Failure);
    }
    case Kind::Repeater:
    case Kind::Retry: {
        // A repeater goes on after successes and a retry goes on after failures, until the count runs out.
        const Status result = run(node.children.front(), deltaSeconds);
        const Status goesOn = node.kind == Kind::Repeater ? Status::Success : Status::Failure;
        if (result == Status::Running) {
            return result;
        }
        if (result != goesOn) {
            node.finished = 0;
            return result;
        }
        if (node.count > 0 && ++node.finished >= node.count) {
            node.finished = 0;
            return result;
        }
        return Status::Running;
    }
    case Kind::Cooldown: {
        if (time < node.readyAt) {
            return Status::Failure;
        }
        const Status result = run(node.children.front(), deltaSeconds);
        if (result != Status::Running) {
            node.readyAt = time + node.seconds;
        }
        return result;
    }
    case Kind::Timeout: {
        node.elapsed += deltaSeconds;
        const Status result = run(node.children.front(), deltaSeconds);
        if (result != Status::Running) {
            node.elapsed = 0.0F;
            return result;
        }
        if (node.elapsed > node.seconds) {
            resetNode(index);
            return Status::Failure;
        }
        return result;
    }
    case Kind::Wait:
        node.elapsed += deltaSeconds;
        if (node.elapsed >= node.seconds) {
            node.elapsed = 0.0F;
            return Status::Success;
        }
        return Status::Running;
    case Kind::Condition:
        return node.condition(blackboard) ? Status::Success : Status::Failure;
    case Kind::Action:
        break;
    }
    return node.action(blackboard, deltaSeconds);
}

} // namespace haylen::ai
