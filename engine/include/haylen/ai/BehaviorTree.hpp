#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/ai/Blackboard.hpp"

namespace haylen::ai {

// A behavior tree built from a description of composites, decorators and leaves, ticked once per update. Sequences and selectors remember their running child and go on from it on the next tick. Leaves are functions of the blackboard, and actions also take the time step.
class BehaviorTree final {
  public:
    enum class Status : std::uint8_t {
        Success,
        Failure,
        Running,
    };

    enum class Kind : std::uint8_t {
        Sequence,
        Selector,
        Parallel,
        Inverter,
        Succeeder,
        Failer,
        Repeater,
        Retry,
        Cooldown,
        Timeout,
        Wait,
        Condition,
        Action,
    };

    using ActionFunction = std::function<Status(Blackboard&, float)>;
    using ConditionFunction = std::function<bool(Blackboard&)>;

    // A node of a tree description, which the static functions below build.
    struct Node {
        Kind kind = Kind::Sequence;
        std::vector<Node> children;
        ActionFunction action;
        ConditionFunction condition;
        int count = 0;
        float seconds = 0.0F;
    };

    // Runs the children in order and fails as soon as one fails.
    [[nodiscard]] static Node sequence(std::vector<Node> children);
    // Runs the children in order and succeeds as soon as one succeeds.
    [[nodiscard]] static Node selector(std::vector<Node> children);
    // Ticks every unfinished child on each tick, succeeds once `successes` children succeeded and fails once too many failed to reach that.
    [[nodiscard]] static Node parallel(std::vector<Node> children, int successes);
    [[nodiscard]] static Node inverter(Node child);
    // Turns a finished child into a success.
    [[nodiscard]] static Node succeeder(Node child);
    // Turns a finished child into a failure.
    [[nodiscard]] static Node failer(Node child);
    // Runs the child again after each success, once per tick, and succeeds after `count` successes or never with 0. A failure of the child fails it.
    [[nodiscard]] static Node repeater(Node child, int count = 0);
    // Runs the child again after each failure, once per tick, and fails after `count` failures or never with 0. A success of the child succeeds it.
    [[nodiscard]] static Node retry(Node child, int count = 0);
    // Fails without running the child until `seconds` of tree time passed since the child last finished.
    [[nodiscard]] static Node cooldown(Node child, float seconds);
    // Fails and resets the child when it keeps running for more than `seconds`.
    [[nodiscard]] static Node timeout(Node child, float seconds);
    // Runs for `seconds` and then succeeds.
    [[nodiscard]] static Node wait(float seconds);
    [[nodiscard]] static Node condition(ConditionFunction predicate);
    [[nodiscard]] static Node action(ActionFunction function);

    [[nodiscard]] static std::optional<Status> statusFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view statusName(Status value) noexcept;

    // Throws `std::invalid_argument` for decorators without exactly one child, leaves without a function or a parallel node whose success count does not fit its children.
    explicit BehaviorTree(Node root);

    // Advances the tree time and ticks the root, which returns to its first child after it finishes.
    Status tick(float deltaSeconds);
    // Forgets the running children, counters and timers, keeping the blackboard.
    void reset();

    [[nodiscard]] Blackboard& getBlackboard() noexcept {
        return blackboard;
    }
    [[nodiscard]] Status getStatus() const noexcept {
        return status;
    }
    [[nodiscard]] float getTime() const noexcept {
        return time;
    }
    [[nodiscard]] std::size_t getNodeCount() const noexcept {
        return nodes.size();
    }

  private:
    static const std::array<std::pair<std::string_view, Status>, 3> kStatusNames;

    struct State {
        Kind kind = Kind::Sequence;
        std::vector<std::uint32_t> children;
        ActionFunction action;
        ConditionFunction condition;
        int count = 0;
        float seconds = 0.0F;
        std::size_t current = 0;
        int finished = 0;
        float elapsed = 0.0F;
        float readyAt = 0.0F;
        std::vector<Status> results;
    };

    [[nodiscard]] static Node decorate(Kind kind, Node child, int count, float seconds);
    std::uint32_t compile(Node node);
    Status run(std::uint32_t index, float deltaSeconds);
    Status runParallel(State& node, float deltaSeconds);
    void resetNode(std::uint32_t index);

    std::vector<State> nodes;
    Blackboard blackboard;
    Status status = Status::Success;
    float time = 0.0F;
};

} // namespace haylen::ai
