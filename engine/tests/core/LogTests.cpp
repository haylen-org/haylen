#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "haylen/core/Log.hpp"
#include "haylen/debug/ObjectCounter.hpp"

namespace haylen::core {

TEST(LogTest, FiltersByLevel) {
    std::vector<std::pair<Log::Level, std::string>> lines;
    const std::uint64_t listener = Log::addListener([&](Log::Level level, std::string_view message) { lines.emplace_back(level, std::string(message)); });

    Log::setLevel(Log::Level::Info);
    Log::debug("hidden {}", 1);
    Log::info("loaded {} textures", 3);
    Log::warning("slow frame");
    Log::error("failed {}", "badly");
    Log::setLevel(Log::Level::Debug);
    Log::debug("visible");
    EXPECT_EQ(Log::getLevel(), Log::Level::Debug);

    Log::removeListener(listener);
    Log::setLevel(Log::Level::Info);

    ASSERT_EQ(lines.size(), 4U);
    EXPECT_EQ(lines[0].first, Log::Level::Info);
    EXPECT_NE(lines[0].second.find("loaded 3 textures"), std::string::npos);
    EXPECT_EQ(lines[1].first, Log::Level::Warning);
    EXPECT_EQ(lines[2].first, Log::Level::Error);
    EXPECT_EQ(lines[3].first, Log::Level::Debug);
}

// A line carries its time, its level and its message, and never the name of the library that writes it.
TEST(LogTest, WritesLinesWithTheirTimeAndLevelOnly) {
    std::string line;
    const std::uint64_t listener = Log::addListener([&](Log::Level, std::string_view message) { line = std::string(message); });
    Log::info("Ready");
    Log::removeListener(listener);

    EXPECT_TRUE(line.starts_with("[")) << line;
    EXPECT_TRUE(line.ends_with("] [info] Ready")) << line;
    EXPECT_EQ(line.find("varn"), std::string::npos) << line;
}

TEST(LogTest, FansLinesOutToListeners) {
    std::vector<std::pair<Log::Level, std::string>> lines;
    const std::uint64_t id = Log::addListener([&lines](Log::Level level, std::string_view line) { lines.emplace_back(level, std::string(line)); });
    Log::warning("storm at {}", "night");
    Log::write(Log::Level::Error, "fire went out");
    Log::removeListener(id);
    Log::info("unheard");

    ASSERT_EQ(lines.size(), 2U);
    EXPECT_EQ(lines[0].first, Log::Level::Warning);
    EXPECT_NE(lines[0].second.find("storm at night"), std::string::npos);
    EXPECT_EQ(lines[1].first, Log::Level::Error);
    EXPECT_NE(lines[1].second.find("fire went out"), std::string::npos);
}

TEST(LogTest, RemovingAListenerWaitsForTheLineItIsWriting) {
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool released = false;
    // clang-format off
    const std::uint64_t id = Log::addListener([&](Log::Level, std::string_view) {
        std::unique_lock lock(mutex);
        entered = true;
        changed.notify_all();
        if (!changed.wait_for(lock, std::chrono::seconds(10), [&] { return released; })) {
            ADD_FAILURE() << "The test never released the listener.";
        }
    });
    // clang-format on
    std::thread writer([] { Log::warning("a line from another thread"); });
    {
        std::unique_lock lock(mutex);
        EXPECT_TRUE(changed.wait_for(lock, std::chrono::seconds(10), [&] { return entered; })) << "The listener never received the line.";
    }

    // A plugin removes its listener before it goes away, so the removal must not return while the listener still runs.
    std::atomic<bool> removed = false;
    // clang-format off
    std::thread remover([&] {
        Log::removeListener(id);
        removed = true;
    });
    // clang-format on
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(removed);
    {
        const std::scoped_lock lock(mutex);
        released = true;
    }

    changed.notify_all();
    writer.join();
    remover.join();
    EXPECT_TRUE(removed);
}

// An `exit()` on another thread, such as the one the iOS simulator calls when its render server dies, runs the destructors of static objects while the frame thread still logs and counts objects. The exit handler, registered before the loop first adds a listener, runs after the listeners would have been destroyed and lets the loop go on for a while.
TEST(LogTest, KeepsWorkingWhileAnotherThreadExits) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    // clang-format off
    EXPECT_EXIT({
        static std::atomic<int> iterations = 0;
        std::atexit([] {
            const int seen = iterations.load();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (iterations.load() < seen + 1000 && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::yield();
            }

            // A loop that stopped while the process exits ends it with another code, which fails the test.
            if (iterations.load() < seen + 1000) {
                std::_Exit(1);
            }
        });
        std::thread([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            std::exit(0);
        }).detach();

        for (;;) {
            Log::removeListener(Log::addListener([](Log::Level, std::string_view) {}));
            const debug::ObjectCounter counter("Probe", debug::ObjectCounter::Kind::Native);
            ++iterations;
        }
    }, testing::ExitedWithCode(0), "");
    // clang-format on
}

} // namespace haylen::core
