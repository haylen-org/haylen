#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/ai/StateMachine.hpp"

namespace haylen::ai {

class StateMachineTest : public ::testing::Test {
  protected:
    // Records every callback of a state under its name.
    [[nodiscard]] static StateMachine::Callbacks makeRecorder(std::vector<std::string>& log, const std::string& name) {
        // clang-format off
        return {
            .enter = [&log, name] { log.push_back("enter " + name); },
            .update = [&log, name](float deltaSeconds) { log.push_back("update " + name + " " + std::to_string(static_cast<int>(deltaSeconds * 10.0F))); },
            .exit = [&log, name] { log.push_back("exit " + name); },
        };
        // clang-format on
    }
};

TEST_F(StateMachineTest, EntersUpdatesAndLeavesStates) {
    std::vector<std::string> log;
    StateMachine machine;
    machine.add("idle", makeRecorder(log, "idle"));
    machine.add("walk", makeRecorder(log, "walk"));
    machine.add("quiet", {});
    machine.onChange = [&log](std::string_view from, std::string_view to) { log.push_back(std::string(from) + ">" + std::string(to)); };

    machine.update(1.0F);
    EXPECT_TRUE(machine.getCurrent().empty());
    EXPECT_TRUE(log.empty());

    machine.change("idle");
    machine.update(0.5F);
    machine.update(0.25F);
    EXPECT_FLOAT_EQ(machine.getElapsed(), 0.75F);
    machine.change("walk");
    EXPECT_EQ(machine.getElapsed(), 0.0F);
    machine.change("walk");
    machine.change("quiet");
    machine.update(0.5F);

    const std::vector<std::string> expected{">idle", "enter idle", "update idle 5", "update idle 2", "exit idle", "idle>walk", "enter walk", "exit walk", "walk>walk", "enter walk", "exit walk", "walk>quiet"};
    EXPECT_EQ(log, expected);
    EXPECT_EQ(machine.getCurrent(), "quiet");
    EXPECT_EQ(machine.getPrevious(), "walk");
    EXPECT_TRUE(machine.has("idle"));
    EXPECT_FALSE(machine.has("run"));
}

TEST_F(StateMachineTest, QueuesChangesRequestedDuringTransitions) {
    std::vector<std::string> log;
    StateMachine machine;
    // clang-format off
    machine.add("a", {
        .enter = [&] {
            log.push_back("enter a");
            machine.change("b");
            machine.change("c");
        },
        .update = {},
        .exit = [&] { log.push_back("exit a"); },
    });
    // clang-format on
    machine.add("b", {.enter = [&] { log.push_back("enter b"); }, .update = {}, .exit = [&] { log.push_back("exit b"); }});
    machine.add("c", {.enter = [&] { log.push_back("enter c"); }, .update = {}, .exit = {}});

    machine.change("a");
    EXPECT_EQ(log, (std::vector<std::string>{"enter a", "exit a", "enter b", "exit b", "enter c"}));
    EXPECT_EQ(machine.getCurrent(), "c");
    EXPECT_EQ(machine.getPrevious(), "b");
}

TEST_F(StateMachineTest, RejectsBadStatesAndRecoversFromFailures) {
    StateMachine machine;
    machine.add("safe", {});
    bool fail = true;
    // clang-format off
    machine.add("fragile", {
        .enter = [&] {
            machine.change("safe");

            if (fail) {
                throw std::runtime_error("broken");
            }
        },
        .update = {},
        .exit = {},
    });
    // clang-format on

    EXPECT_THROW(machine.add("safe", {}), std::invalid_argument);
    EXPECT_THROW(machine.add("", {}), std::invalid_argument);
    EXPECT_THROW(machine.change("missing"), std::invalid_argument);

    EXPECT_THROW(machine.change("fragile"), std::runtime_error);
    EXPECT_EQ(machine.getCurrent(), "fragile");

    // The change queued before the failure was dropped, so the next change starts clean.
    fail = false;
    machine.change("safe");
    EXPECT_EQ(machine.getCurrent(), "safe");
    EXPECT_EQ(machine.getPrevious(), "fragile");
}

} // namespace haylen::ai
