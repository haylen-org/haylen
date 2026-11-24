#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/debug/TraceRecorder.hpp"
#include "haylen/debug/TraceSpan.hpp"

namespace haylen::debug {

class TraceRecorderTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::vector<haylen::core::Json> spansOf(const haylen::core::Json& trace) {
        std::vector<haylen::core::Json> spans;
        for (const haylen::core::Json& event : trace.at("traceEvents")) {
            if (event.at("ph") == "X") {
                spans.push_back(event);
            }
        }

        return spans;
    }

    [[nodiscard]] static std::string laneName(const haylen::core::Json& trace, int lane) {
        for (const haylen::core::Json& event : trace.at("traceEvents")) {
            if (event.at("ph") == "M" && event.at("name") == "thread_name" && event.at("tid") == lane) {
                return event.at("args").at("name");
            }
        }

        return {};
    }
};

TEST_F(TraceRecorderTest, WritesNestedScopesWithALanePerThread) {
    double now = 10.0;
    Profiler profiler(8, [&now] { return now; });
    auto trace = std::make_shared<TraceRecorder>();
    trace->nameThread("frame");
    profiler.setTraceRecorder(trace);

    profiler.beginFrame(0.0);
    profiler.beginScope("update");
    now += 0.002;
    profiler.beginScope("physics");
    now += 0.001;
    profiler.endScope();
    profiler.endScope();
    now += 0.001;
    profiler.endFrame();
    profiler.setTraceRecorder(nullptr);
    profiler.beginFrame(0.0);
    profiler.endFrame();

    std::thread worker([&trace] { trace->record("job", TraceRecorder::now(), TraceRecorder::now() + 0.0005); });
    worker.join();
    trace->recordGpu("gpuFrame", TraceRecorder::now(), 0.004);

    const haylen::core::Json parsed = haylen::core::Json::parse(trace->toJson());
    EXPECT_EQ(parsed.at("displayTimeUnit"), "ms");
    const std::vector<haylen::core::Json> spans = spansOf(parsed);
    ASSERT_EQ(spans.size(), 5U);
    EXPECT_EQ(spans[0].at("name"), "physics");
    EXPECT_EQ(spans[1].at("name"), "update");
    EXPECT_EQ(spans[2].at("name"), "frame");
    EXPECT_NEAR(spans[0].at("dur").get<double>(), 1000.0, 0.01);
    EXPECT_NEAR(spans[1].at("dur").get<double>(), 3000.0, 0.01);
    EXPECT_NEAR(spans[1].at("ts").get<double>() + 2000.0, spans[0].at("ts").get<double>(), 0.01);
    EXPECT_EQ(spans[0].at("tid"), 1);
    EXPECT_EQ(laneName(parsed, 1), "frame");

    EXPECT_EQ(spans[3].at("name"), "job");
    EXPECT_EQ(spans[3].at("tid"), 2);
    EXPECT_EQ(laneName(parsed, 2), "thread 2");
    EXPECT_EQ(spans[4].at("tid"), 0);
    EXPECT_EQ(laneName(parsed, 0), "gpu");
    EXPECT_NEAR(spans[4].at("dur").get<double>(), 4000.0, 0.01);
}

// The name of a scope comes from the app and may hold any bytes, which the trace replaces so it stays JSON.
TEST_F(TraceRecorderTest, WritesNamesThatAreNotUtf8) {
    TraceRecorder trace(4);
    trace.record("a\xFF"
                 "b",
                 0.0, 1.0);

    const haylen::core::Json parsed = haylen::core::Json::parse(trace.toJson());
    ASSERT_FALSE(spansOf(parsed).empty());
    EXPECT_EQ(spansOf(parsed).front().at("name"), "a\xEF\xBF\xBD"
                                                  "b");
}

TEST_F(TraceRecorderTest, DropsSpansBeyondItsCapacity) {
    TraceRecorder trace(3);
    for (int index = 0; index < 5; ++index) {
        trace.record("say \"hi\"", 0.0, 1.0);
    }

    EXPECT_EQ(trace.size(), 3U);
    EXPECT_EQ(trace.getDropped(), 2U);
    const haylen::core::Json parsed = haylen::core::Json::parse(trace.toJson());
    EXPECT_EQ(parsed.at("otherData").at("dropped"), 2);
    ASSERT_FALSE(spansOf(parsed).empty());
    EXPECT_EQ(spansOf(parsed).front().at("name"), "say \"hi\"");
}

TEST_F(TraceRecorderTest, RecordsSpansIntoTheRecorderOfTheThreadAlone) {
    EXPECT_EQ(TraceRecorder::getThreadRecorder(), nullptr);
    TraceRecorder trace;
    TraceRecorder::setThreadRecorder(&trace);
    { const TraceSpan span("read", "content/a.png"); }
    TraceRecorder::setThreadRecorder(nullptr);
    { const TraceSpan span("read", "content/b.png"); }

    const std::vector<haylen::core::Json> spans = spansOf(haylen::core::Json::parse(trace.toJson()));
    ASSERT_EQ(spans.size(), 1U);
    EXPECT_EQ(spans.front().at("name"), "read content/a.png");
}

} // namespace haylen::debug
