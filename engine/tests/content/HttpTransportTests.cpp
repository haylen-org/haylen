#include <gtest/gtest.h>

#include <Poco/Net/ServerSocket.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "content/HttpTransport.hpp"
#include "content/RangePlanner.hpp"
#include "support/ContentServer.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"
#include "support/VarnRuntime.hpp"

namespace haylen::content {

class HttpTransportTest : public ::testing::Test {
  protected:
    static constexpr std::size_t kFileSize = 300 * 1024;

    struct Outcome {
        std::optional<HttpTransport::Start> start;
        std::vector<std::uint8_t> bytes;
        std::optional<Error> error;
        bool done = false;
    };

    HttpTransportTest() {
        file = test::TestFiles::randomBytes(kFileSize, 5);
        folder.write("packs/ab/pack.hpak", std::string(file.begin(), file.end()));
    }

    HttpTransport::Transfer fetch(HttpTransport::Request request, Outcome& outcome) {
        // clang-format off
        return transport.fetch(std::move(request), {
            .onStart = [&outcome](const HttpTransport::Start& start) { outcome.start = start; },
            .onBytes = [&outcome](std::span<const std::uint8_t> bytes) { outcome.bytes.insert(outcome.bytes.end(), bytes.begin(), bytes.end()); },
            .onComplete = [&outcome](std::optional<Error> error) {
                outcome.error = std::move(error);
                outcome.done = true;
            },
        });
        // clang-format on
    }

    Outcome run(HttpTransport::Request request) {
        Outcome outcome;
        fetch(std::move(request), outcome);
        EXPECT_TRUE(runtime.pumpUntil([&outcome] { return outcome.done; }));
        return outcome;
    }

    [[nodiscard]] std::string url(const std::string& path = "packs/ab/pack.hpak") const {
        return server.getUrl() + path;
    }

    [[nodiscard]] std::vector<std::uint8_t> slice(std::uint64_t offset, std::uint64_t size) const {
        return {file.begin() + static_cast<std::ptrdiff_t>(offset), file.begin() + static_cast<std::ptrdiff_t>(offset + size)};
    }

    test::TemporaryDirectory folder;
    std::vector<std::uint8_t> file;
    test::ContentServer server{folder.getPath()};
    test::VarnRuntime runtime;
    HttpTransport transport{runtime.getRuntime()};
};

TEST_F(HttpTransportTest, FetchesAWholeFile) {
    const Outcome outcome = run({.url = url()});

    ASSERT_FALSE(outcome.error) << outcome.error->what();
    EXPECT_EQ(outcome.bytes, file);
    ASSERT_TRUE(outcome.start);
    EXPECT_EQ(outcome.start->fileSize, kFileSize);
    EXPECT_FALSE(outcome.start->validator.empty());
}

TEST_F(HttpTransportTest, FetchesExactlyTheRangeItAsksFor) {
    const Outcome outcome = run({.url = url(), .offset = 1000, .size = 70000});

    ASSERT_FALSE(outcome.error) << outcome.error->what();
    EXPECT_EQ(outcome.bytes, slice(1000, 70000));
    EXPECT_EQ(outcome.start->fileSize, kFileSize);
    const std::vector<test::ContentServer::Exchange> exchanges = server.getExchanges();
    ASSERT_EQ(exchanges.size(), 1U);
    EXPECT_EQ(exchanges[0].first, 1000U);
    EXPECT_EQ(exchanges[0].last, 70999U);
    EXPECT_EQ(exchanges[0].bytes, 70000U);
}

TEST_F(HttpTransportTest, ResumesARangeFromTheBytesItHolds) {
    const Outcome first = run({.url = url(), .offset = 0, .size = 4096});
    const Outcome outcome = run({.url = url(), .offset = 2000, .size = 50000, .held = 30000, .validator = first.start->validator});

    ASSERT_FALSE(outcome.error) << outcome.error->what();
    EXPECT_TRUE(outcome.start->resumed);
    EXPECT_EQ(outcome.bytes, slice(32000, 20000));
    EXPECT_EQ(server.getExchanges().back().first, 32000U);
}

TEST_F(HttpTransportTest, StartsARangeAgainWhenTheFileChanged) {
    const Outcome first = run({.url = url(), .offset = 0, .size = 4096});
    server.touch("packs/ab/pack.hpak");
    server.clearExchanges();

    const Outcome outcome = run({.url = url(), .offset = 2000, .size = 50000, .held = 30000, .validator = first.start->validator});

    ASSERT_FALSE(outcome.error) << outcome.error->what();
    EXPECT_FALSE(outcome.start->resumed);
    EXPECT_EQ(outcome.bytes, slice(2000, 50000));
    // The server records an exchange when its answer ends, and the whole file that the client stopped reading may end after the range that followed it.
    const std::vector<test::ContentServer::Exchange> exchanges = server.getExchanges();
    ASSERT_EQ(exchanges.size(), 2U);
    EXPECT_TRUE(std::ranges::any_of(exchanges, [](const test::ContentServer::Exchange& exchange) { return exchange.status == 200 && !exchange.first; }));
    EXPECT_TRUE(std::ranges::any_of(exchanges, [](const test::ContentServer::Exchange& exchange) { return exchange.status == 206 && exchange.first == 2000U; }));
}

TEST_F(HttpTransportTest, NeverResumesWithoutAValidator) {
    const Outcome outcome = run({.url = url(), .offset = 2000, .size = 50000, .held = 30000});

    ASSERT_FALSE(outcome.error) << outcome.error->what();
    EXPECT_FALSE(outcome.start->resumed);
    EXPECT_EQ(outcome.bytes, slice(2000, 50000));
}

TEST_F(HttpTransportTest, RefusesAServerThatIgnoresRanges) {
    server.setIgnoresRanges(true);
    const Outcome outcome = run({.url = url(), .offset = 1000, .size = 2000});

    ASSERT_TRUE(outcome.error);
    EXPECT_EQ(outcome.error->getCode(), Error::Code::RangeUnsupported);
    EXPECT_FALSE(outcome.error->isRetryable());
    EXPECT_TRUE(outcome.bytes.empty());
}

TEST_F(HttpTransportTest, RefusesAnotherRangeThanItAskedFor) {
    server.setShiftsRanges(true);
    const Outcome outcome = run({.url = url(), .offset = 1000, .size = 2000});

    ASSERT_TRUE(outcome.error);
    EXPECT_EQ(outcome.error->getCode(), Error::Code::RangeUnsupported);
    EXPECT_TRUE(outcome.bytes.empty());
}

TEST_F(HttpTransportTest, ReportsTheStatusOfAFailedRequest) {
    server.setStatus("packs/ab/pack.hpak", 503);
    const Outcome missing = run({.url = url("packs/ab/other.hpak"), .offset = 0, .size = 10});
    const Outcome busy = run({.url = url(), .offset = 0, .size = 10});

    ASSERT_TRUE(missing.error);
    EXPECT_EQ(missing.error->getCode(), Error::Code::UnexpectedHttpStatus);
    EXPECT_EQ(missing.error->getDetails().httpStatus, 404);
    EXPECT_FALSE(missing.error->isRetryable());
    ASSERT_TRUE(busy.error);
    EXPECT_EQ(busy.error->getDetails().httpStatus, 503);
    EXPECT_TRUE(busy.error->isRetryable());
}

TEST_F(HttpTransportTest, ReportsADownloadThatEndsEarly) {
    server.dropAfter(10000);
    const Outcome outcome = run({.url = url(), .offset = 0, .size = 100000});

    ASSERT_TRUE(outcome.error);
    EXPECT_EQ(outcome.error->getCode(), Error::Code::UpdateInterrupted);
    EXPECT_TRUE(outcome.error->isRetryable());
    EXPECT_EQ(outcome.bytes, slice(0, outcome.bytes.size()));
}

TEST_F(HttpTransportTest, ReportsAServerThatCannotBeReached) {
    std::string closed;
    {
        const Poco::Net::ServerSocket socket(Poco::Net::SocketAddress("127.0.0.1", 0));
        closed = "http://127.0.0.1:" + std::to_string(socket.address().port()) + "/pack.hpak";
    }
    const Outcome outcome = run({.url = closed, .offset = 0, .size = 10});

    ASSERT_TRUE(outcome.error);
    EXPECT_EQ(outcome.error->getCode(), Error::Code::NetworkOffline);
    EXPECT_TRUE(outcome.error->isRetryable());
}

TEST_F(HttpTransportTest, EndsARequestThatTakesTooLong) {
    server.setStalls(true);
    const Outcome outcome = run({.url = url(), .offset = 0, .size = 10, .timeout = std::chrono::milliseconds(200)});

    ASSERT_TRUE(outcome.error);
    EXPECT_EQ(outcome.error->getCode(), Error::Code::NetworkTimeout);
}

TEST_F(HttpTransportTest, LetsGoOfARequestThatNeverCompletesWhenItEnds) {
    server.setStalls(true);
    const auto sentinel = std::make_shared<int>(0);
    {
        HttpTransport ending(runtime.getRuntime());
        const HttpTransport::Transfer transfer = ending.fetch({.url = url()}, {.onComplete = [sentinel](std::optional<Error>) {}});
        ASSERT_TRUE(runtime.pumpUntil([this] { return server.getAnswering() > 0; }));
        transfer.cancel();
    }
    server.setStalls(false);
    EXPECT_EQ(sentinel.use_count(), 1) << "The callbacks of the request end with the transport.";
}

TEST_F(HttpTransportTest, CancelsFromAnotherThread) {
    server.setStalls(true);
    Outcome outcome;
    const HttpTransport::Transfer transfer = fetch({.url = url(), .offset = 0, .size = 10}, outcome);
    EXPECT_TRUE(transfer.isActive());
    std::thread([transfer] { transfer.cancel(); }).join();

    ASSERT_TRUE(runtime.pumpUntil([&outcome] { return outcome.done; }));
    ASSERT_TRUE(outcome.error);
    EXPECT_EQ(outcome.error->getCode(), Error::Code::NetworkCancelled);
    EXPECT_FALSE(transfer.isActive());
}

TEST_F(HttpTransportTest, GrowsTheDeadlineWithTheBytes) {
    EXPECT_LT(HttpTransport::getTimeout(1024), HttpTransport::getTimeout(std::uint64_t{64} << 20));
    EXPECT_GE(HttpTransport::getTimeout(0), std::chrono::seconds(30));
}

TEST(RangePlannerTest, JoinsRecordsThatLieCloseTogether) {
    const std::vector<RangePlanner::Span> records = {{160, 1000}, {1200, 500}, {1800, 300}};
    const std::vector<RangePlanner::Range> ranges = RangePlanner::plan(records);

    ASSERT_EQ(ranges.size(), 1U);
    EXPECT_EQ(ranges[0].offset, 160U);
    EXPECT_EQ(ranges[0].size, 1940U);
    EXPECT_EQ(ranges[0].count, 3U);
}

TEST(RangePlannerTest, SplitsAtLargeGapsAndAtTheLimitOfARequest) {
    constexpr std::uint64_t kChunk = 4 * 1024 * 1024;
    const std::vector<RangePlanner::Span> records = {{160, 100}, {160 + RangePlanner::kMaximumGap + 200, 100}, {kChunk, kChunk}, {2 * kChunk, kChunk}, {3 * kChunk, kChunk}, {4 * kChunk, kChunk}, {5 * kChunk, kChunk}};
    const std::vector<RangePlanner::Range> ranges = RangePlanner::plan(records);

    ASSERT_EQ(ranges.size(), 4U);
    EXPECT_EQ(ranges[0].count, 1U);
    EXPECT_EQ(ranges[1].first, 1U);
    EXPECT_EQ(ranges[2].first, 2U);
    EXPECT_EQ(ranges[2].count, 4U);
    EXPECT_EQ(ranges[2].size, RangePlanner::kMaximumRange);
    EXPECT_EQ(ranges[3].first, 6U);
}

} // namespace haylen::content
