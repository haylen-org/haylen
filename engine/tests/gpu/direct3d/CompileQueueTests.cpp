#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <optional>

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

} // namespace haylen::gpu::direct3d
