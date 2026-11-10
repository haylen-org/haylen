#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <thread>

#include "gpu/direct3d/CompileQueue.hpp"

namespace haylen::gpu::direct3d {

TEST(CompileQueueTest, RunsEveryJobItIsGiven) {
    CompileQueue queue(2);
    std::atomic<int> ran = 0;
    std::promise<void> done;
    constexpr int kJobs = 8;
    for (int index = 0; index < kJobs; ++index) {
        // clang-format off
        queue.post([&ran, &done] {
            if (++ran == kJobs) {
                done.set_value();
            }
        });
        // clang-format on
    }
    EXPECT_EQ(done.get_future().wait_for(std::chrono::seconds(10)), std::future_status::ready);
}

// A compilation that runs while the app quits never holds the quit up, and the ones that did not start never run.
TEST(CompileQueueTest, GoesAwayWithoutWaitingForTheJobThatRuns) {
    auto release = std::make_shared<std::promise<void>>();
    auto started = std::make_shared<std::promise<void>>();
    auto finished = std::make_shared<std::promise<void>>();
    auto dropped = std::make_shared<std::atomic<bool>>(false);
    std::shared_future<void> gate = release->get_future().share();

    std::optional<CompileQueue> queue(std::in_place, 1);
    // clang-format off
    queue->post([gate, started, finished] {
        started->set_value();
        gate.wait();
        finished->set_value();
    });
    // clang-format on
    queue->post([dropped] { *dropped = true; });
    ASSERT_EQ(started->get_future().wait_for(std::chrono::seconds(10)), std::future_status::ready);

    const auto before = std::chrono::steady_clock::now();
    queue.reset();
    EXPECT_LT(std::chrono::steady_clock::now() - before, std::chrono::seconds(1));

    release->set_value();
    EXPECT_EQ(finished->get_future().wait_for(std::chrono::seconds(10)), std::future_status::ready);
    EXPECT_FALSE(*dropped);
}

// A job queued while the only free thread is about to take another one starts a thread of its own, so it never waits for a long compilation that runs.
TEST(CompileQueueTest, StartsAThreadWhenTheJobsOutnumberTheFreeThreads) {
    CompileQueue queue(2);
    std::promise<void> warmed;
    queue.post([&warmed] { warmed.set_value(); });
    ASSERT_EQ(warmed.get_future().wait_for(std::chrono::seconds(10)), std::future_status::ready);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto release = std::make_shared<std::promise<void>>();
    std::shared_future<void> gate = release->get_future().share();
    auto first = std::make_shared<std::promise<void>>();
    auto second = std::make_shared<std::promise<void>>();
    // clang-format off
    queue.post([gate, first] {
        first->set_value();
        gate.wait();
    });
    queue.post([gate, second] {
        second->set_value();
        gate.wait();
    });
    // clang-format on

    EXPECT_EQ(first->get_future().wait_for(std::chrono::seconds(10)), std::future_status::ready);
    EXPECT_EQ(second->get_future().wait_for(std::chrono::seconds(10)), std::future_status::ready);
    release->set_value();
}

// A job that waits behind a running one comes back to its caller, which runs it at once, while a job that started stays with its thread.
TEST(CompileQueueTest, WithdrawsOnlyTheJobsThatDidNotStart) {
    CompileQueue queue(1);
    auto release = std::make_shared<std::promise<void>>();
    std::shared_future<void> gate = release->get_future().share();
    auto started = std::make_shared<std::promise<void>>();
    auto waiting = std::make_shared<std::atomic<bool>>(false);

    // clang-format off
    const std::uint64_t running = queue.post([gate, started] {
        started->set_value();
        gate.wait();
    });
    // clang-format on
    const std::uint64_t queued = queue.post([waiting] { *waiting = true; });
    ASSERT_EQ(started->get_future().wait_for(std::chrono::seconds(10)), std::future_status::ready);

    EXPECT_FALSE(queue.withdraw(running));
    EXPECT_TRUE(queue.withdraw(queued));
    EXPECT_FALSE(queue.withdraw(queued));

    std::promise<void> after;
    queue.post([&after] { after.set_value(); });
    release->set_value();
    EXPECT_EQ(after.get_future().wait_for(std::chrono::seconds(10)), std::future_status::ready);
    EXPECT_FALSE(*waiting);
}

} // namespace haylen::gpu::direct3d
