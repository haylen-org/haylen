#include <gtest/gtest.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "haylen/core/Json.hpp"
#include "support/LuaFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::debug {

TEST(TraceLuaTest, TracesFramesAndJobsIntoAFileOffTheFrameThread) {
    test::LuaFixture fixture;
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

    const haylen::core::Json trace = haylen::core::Json::parse(fixture.lua("return require('haylen.storage').readText('traces/frames.json')"));
    std::set<std::string> names;
    int frames = 0;
    for (const haylen::core::Json& event : trace.at("traceEvents")) {
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

TEST(TraceLuaTest, NamesTheReadDecodeAndCreationOfEachAssetLoad) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(8, 4, 0xFFFFFFFFU);
    test::LuaFixture fixture({{"content/images/hero.png", std::string(image.begin(), image.end())}});
    // clang-format off
    fixture.runLua(R"(
        debugging = require('haylen.debug')
        debugging.startTrace()
        require('async').spawn(function()
            require('haylen.assets').loadAsync('images/hero.png'):await()
            written = debugging.stopTraceAsync('traces/loads.json'):await()
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return tostring(written ~= nil)") == "true"; }));

    const haylen::core::Json trace = haylen::core::Json::parse(fixture.lua("return require('haylen.storage').readText('traces/loads.json')"));
    std::map<std::string, int> lanes;
    for (const haylen::core::Json& event : trace.at("traceEvents")) {
        if (event.at("ph") == "X") {
            lanes.emplace(event.at("name").get<std::string>(), event.at("tid").get<int>());
        }
    }

    ASSERT_TRUE(lanes.contains("read content/images/hero.png"));
    ASSERT_TRUE(lanes.contains("decode images/hero.png"));
    ASSERT_TRUE(lanes.contains("create images/hero.png"));
    EXPECT_NE(lanes.at("read content/images/hero.png"), lanes.at("create images/hero.png"));
    EXPECT_NE(lanes.at("decode images/hero.png"), lanes.at("create images/hero.png"));
}

} // namespace haylen::lua::debug
