#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "haylen/core/JobSystem.hpp"
#include "haylen/core/Log.hpp"
#include "support/VarnRuntime.hpp"

namespace haylen::core {

TEST(JobSystemTest, RunDeliversValuesOnTheFrameThread) {
    test::VarnRuntime varn;
    core::JobSystem jobs(varn.getRuntime(), [](const std::string& message) { FAIL() << message; });
    const auto frameThread = std::this_thread::get_id();

    std::atomic<bool> done{false};
    // clang-format off
    jobs.run([] { return 21 * 2; }, [&](core::JobSystem::Result<int> result) {
        EXPECT_TRUE(result.isOk());
        EXPECT_EQ(*result.value, 42);
        EXPECT_EQ(std::this_thread::get_id(), frameThread);
        done = true;
    });
    // clang-format on

    ASSERT_TRUE(varn.pumpUntil([&] { return done.load(); }));
}

TEST(JobSystemTest, RunReportsExceptions) {
    test::VarnRuntime varn;
    core::JobSystem jobs(varn.getRuntime(), [](const std::string& message) { FAIL() << message; });

    std::string error;
    std::string unknownError;
    jobs.run([]() -> int { throw std::runtime_error("decode failed"); }, [&](core::JobSystem::Result<int> result) { error = result.error; });
    jobs.run([]() -> int { throw 7; }, [&](core::JobSystem::Result<int> result) { unknownError = result.error; });

    ASSERT_TRUE(varn.pumpUntil([&] { return !error.empty() && !unknownError.empty(); }));
    EXPECT_EQ(error, "decode failed");
    EXPECT_NE(unknownError.find("unknown"), std::string::npos);
}

TEST(JobSystemTest, PostAndPostIoRunInBackground) {
    test::VarnRuntime varn;
    core::JobSystem jobs(varn.getRuntime(), [](const std::string& message) { FAIL() << message; });

    std::atomic<int> count{0};
    jobs.post([&] { ++count; });
    jobs.postIo([&] { ++count; });
    jobs.postToFrame([&] { ++count; });

    ASSERT_TRUE(varn.pumpUntil([&] { return count.load() == 3; }));
    EXPECT_GE(jobs.getWorkerCount(), 1U);
}

TEST(JobSystemTest, ReportsExceptionsThatEscapePostedWork) {
    test::VarnRuntime varn;
    std::vector<std::string> errors;
    core::JobSystem jobs(varn.getRuntime(), [&](const std::string& message) { errors.push_back(message); });

    jobs.post([] { throw std::runtime_error("worker failed"); });
    jobs.postIo([] { throw 3; });
    jobs.postToFrame([] { throw std::logic_error("frame failed"); });

    ASSERT_TRUE(varn.pumpUntil([&] { return errors.size() == 3; }));
    std::sort(errors.begin(), errors.end());
    EXPECT_EQ(errors[0], "A job failed with an unknown exception.");
    EXPECT_EQ(errors[1], "frame failed");
    EXPECT_EQ(errors[2], "worker failed");
}

TEST(JobSystemTest, ParallelForCoversTheWholeRange) {
    test::VarnRuntime varn;
    core::JobSystem jobs(varn.getRuntime(), [](const std::string& message) { FAIL() << message; });

    std::vector<int> values(10000, 0);
    // clang-format off
    jobs.parallelFor(0, values.size(), 64, [&](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            values[index] = static_cast<int>(index);
        }
    });
    // clang-format on

    std::vector<int> expected(values.size());
    std::iota(expected.begin(), expected.end(), 0);
    EXPECT_EQ(values, expected);

    int calls = 0;
    jobs.parallelFor(5, 5, 1, [&](std::size_t, std::size_t) { ++calls; });
    // clang-format off
    jobs.parallelFor(0, 3, 100, [&](std::size_t begin, std::size_t end) {
        EXPECT_EQ(begin, 0U);
        EXPECT_EQ(end, 3U);
        ++calls;
    });
    // clang-format on
    EXPECT_EQ(calls, 1);
}

TEST(JobSystemTest, ParallelForPropagatesTheFirstException) {
    test::VarnRuntime varn;
    core::JobSystem jobs(varn.getRuntime(), [](const std::string& message) { FAIL() << message; });

    // clang-format off
    const auto failInWorkers = [](std::size_t begin, std::size_t) {
        if (begin > 0) {
            throw std::runtime_error("worker chunk failed");
        }
    };
    const auto failInCaller = [](std::size_t begin, std::size_t) {
        if (begin == 0) {
            throw std::logic_error("caller chunk failed");
        }
    };
    // clang-format on

    EXPECT_THROW(jobs.parallelFor(0, 1000, 1, failInWorkers), std::runtime_error);
    EXPECT_THROW(jobs.parallelFor(0, 1000, 1, failInCaller), std::logic_error);
}

TEST(JobSystemTest, DiscardsQueuedWorkWithoutRunningIt) {
    test::VarnRuntime varn;
    core::JobSystem jobs(varn.getRuntime(), [](const std::string& message) { FAIL() << message; });

    // Every worker waits at the gate, so the next job stays in the queue.
    std::atomic<std::size_t> started{0};
    std::atomic<bool> open{false};
    for (std::size_t worker = 0; worker < jobs.getWorkerCount(); ++worker) {
        // clang-format off
        jobs.post([&] {
            ++started;
            while (!open) {
                std::this_thread::yield();
            }
        });
        // clang-format on
    }
    ASSERT_TRUE(varn.pumpUntil([&] { return started.load() == jobs.getWorkerCount(); }));

    auto held = std::make_shared<int>(0);
    const std::weak_ptr<int> watched = held;
    std::atomic<bool> ran{false};
    jobs.post([&ran, sentinel = std::move(held)] { ran = sentinel != nullptr; });
    jobs.discardQueued();
    EXPECT_TRUE(watched.expired());

    open = true;
    jobs.post([&] { ++started; });
    ASSERT_TRUE(varn.pumpUntil([&] { return started.load() == jobs.getWorkerCount() + 1; }));
    EXPECT_FALSE(ran);
}

TEST(LogTest, FiltersByLevel) {
    std::vector<std::pair<core::Log::Level, std::string>> lines;
    const std::uint64_t listener = core::Log::addListener([&](core::Log::Level level, std::string_view message) { lines.emplace_back(level, std::string(message)); });

    core::Log::setLevel(core::Log::Level::Info);
    core::Log::debug("hidden {}", 1);
    core::Log::info("loaded {} textures", 3);
    core::Log::warning("slow frame");
    core::Log::error("failed {}", "badly");
    core::Log::setLevel(core::Log::Level::Debug);
    core::Log::debug("visible");
    EXPECT_EQ(core::Log::getLevel(), core::Log::Level::Debug);

    core::Log::removeListener(listener);
    core::Log::setLevel(core::Log::Level::Info);

    ASSERT_EQ(lines.size(), 4U);
    EXPECT_EQ(lines[0].first, core::Log::Level::Info);
    EXPECT_NE(lines[0].second.find("loaded 3 textures"), std::string::npos);
    EXPECT_EQ(lines[1].first, core::Log::Level::Warning);
    EXPECT_EQ(lines[2].first, core::Log::Level::Error);
    EXPECT_EQ(lines[3].first, core::Log::Level::Debug);
}

} // namespace haylen::core
