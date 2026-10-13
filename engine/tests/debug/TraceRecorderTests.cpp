#include <gtest/gtest.h>

#include <cstddef>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/debug/TraceRecorder.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::debug {

class TraceRecorderTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::vector<core::Json> spansOf(const core::Json& trace) {
        std::vector<core::Json> spans;
        for (const core::Json& event : trace.at("traceEvents")) {
            if (event.at("ph") == "X") {
                spans.push_back(event);
            }
        }
        return spans;
    }

    [[nodiscard]] static std::string laneName(const core::Json& trace, int lane) {
        for (const core::Json& event : trace.at("traceEvents")) {
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

    const core::Json parsed = core::Json::parse(trace->toJson());
    EXPECT_EQ(parsed.at("displayTimeUnit"), "ms");
    const std::vector<core::Json> spans = spansOf(parsed);
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

TEST_F(TraceRecorderTest, DropsSpansBeyondItsCapacity) {
    TraceRecorder trace(3);
    for (int index = 0; index < 5; ++index) {
        trace.record("say \"hi\"", 0.0, 1.0);
    }
    EXPECT_EQ(trace.size(), 3U);
    EXPECT_EQ(trace.getDropped(), 2U);
    const core::Json parsed = core::Json::parse(trace.toJson());
    EXPECT_EQ(parsed.at("otherData").at("dropped"), 2);
    EXPECT_EQ(spansOf(parsed).front().at("name"), "say \"hi\"");
}

TEST(TraceLuaTest, TracesFramesAndJobsIntoAFileOffTheFrameThread) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        debugging = require('haylen.debug')
        local net = require('haylen.net')
        debugging.startTrace({frames = 4})
        require('async').spawn(function()
            net.decodeMessagePackAsync(net.encodeMessagePack({1, 2, 3})):await()
            decoded = true
        end)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return tostring(debugging.tracing())"), "true");
    EXPECT_NE(fixture.lua("debugging.startTrace()").find("A trace is already recording."), std::string::npos);
    fixture.frames(6);
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return tostring(decoded)") == "true"; }));
    EXPECT_EQ(fixture.lua("return tostring(debugging.tracing())"), "false");

    // clang-format off
    fixture.runLua(R"(
        require('async').spawn(function()
            written = debugging.stopTraceAsync('traces/frames.json'):await()
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return tostring(written ~= nil)") == "true"; }));
    EXPECT_EQ(fixture.lua("return written.path .. ' ' .. tostring(written.spans > 0) .. ' ' .. written.dropped"), "traces/frames.json true 0");

    const core::Json trace = core::Json::parse(fixture.lua("return require('haylen.storage').readText('traces/frames.json')"));
    std::set<std::string> names;
    int frames = 0;
    for (const core::Json& event : trace.at("traceEvents")) {
        if (event.at("ph") == "X") {
            names.insert(event.at("name").get<std::string>());
            frames += event.at("name") == "frame" ? 1 : 0;
        }
    }
    EXPECT_EQ(frames, 4);
    EXPECT_TRUE(names.contains("update"));
    EXPECT_TRUE(names.contains("job"));

    EXPECT_NE(fixture.lua("debugging.stopTraceAsync('traces/again.json')").find("No trace is recording. Start one with \"debug.startTrace\"."), std::string::npos);
    EXPECT_NE(fixture.lua("debugging.startTrace({frames = 0})").find("A trace records at least 1 frame."), std::string::npos);
    EXPECT_NE(fixture.lua("debugging.startTrace({seconds = 2})").find("Unknown option \"seconds\"."), std::string::npos);
}

} // namespace haylen::debug
