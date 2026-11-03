#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <string>

#include "haylen/core/Engine.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::net {

// A download of the `http` module writes its body to a file as it arrives, so an app that fetches a file far larger than its memory holds never gathers it on the frame thread, and one that fails leaves no partial file.
class HttpDownloadTest : public ::testing::Test {
  protected:
    static constexpr std::size_t kMegabyte = 1024 * 1024;

    void SetUp() override {
        fixture.runLua("target = " + quoted(folder.getPath() / "pack.bin"));
        // clang-format off
        fixture.runLua(R"(
            async = require('async')
            http = require('http')
            local piece = string.rep('h', 64 * 1024)
            local app = http.createApp()
            app:get('/pack', function(ctx)
                ctx:header('Content-Length', tostring(#piece * 512))
                for _ = 1, 512 do
                    if not ctx:write(piece):await() then
                        return
                    end
                end
            end)
            base = 'http://127.0.0.1:' .. app:listen({host = '127.0.0.1', port = 0}).port
        )");
        // clang-format on
    }

    static std::string quoted(const std::filesystem::path& path) {
        return "'" + path.generic_string() + "'";
    }

    test::TemporaryDirectory folder;
    test::EngineFixture fixture;
};

TEST_F(HttpDownloadTest, WritesALargeBodyToAFileInBoundedMemory) {
    // clang-format off
    fixture.runLua(R"(
        async.spawn(function()
            result, failure = http.client.download({url = base .. '/pack', path = target, timeoutSeconds = 60, maxResponseBytes = 64 * 1024 * 1024}):await()
            finished = true
        end)
    )");
    // clang-format on

    std::size_t largest = 0;
    {
        const test::AllocationTracker tracker;
        ASSERT_TRUE(fixture.frameUntil([this] { return fixture.lua("return tostring(finished)") == "true"; }, std::chrono::seconds(60)));
        largest = tracker.getLargest();
    }
    EXPECT_LT(largest, kMegabyte) << "No allocation of the frame thread holds more than a piece of the body.";
    EXPECT_EQ(fixture.lua("return failure or result.size"), std::to_string(32 * kMegabyte));
    EXPECT_EQ(std::filesystem::file_size(folder.getPath() / "pack.bin"), 32 * kMegabyte);
    EXPECT_FALSE(std::filesystem::exists(folder.getPath() / "pack.bin.part"));
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST_F(HttpDownloadTest, LeavesNoPartialFileWhenItsTaskEnds) {
    // clang-format off
    fixture.runLua(R"(
        task = async.spawn(function()
            local scope <close> = http.client.controller()
            pending = http.client.download({url = base .. '/pack', path = target, timeoutSeconds = 60, maxResponseBytes = 64 * 1024 * 1024, controller = scope, onProgress = function()
                started = true
            end})
            pending:catch(function(reason) failure = reason end)
            pending:await()
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([this] { return fixture.lua("return tostring(started)") == "true"; }));

    fixture.runLua("task.cancel()");
    ASSERT_TRUE(fixture.frameUntil([this] { return fixture.lua("return tostring(failure)") != "nil"; }));
    EXPECT_EQ(fixture.lua("return failure"), "The request was cancelled.");
    EXPECT_FALSE(std::filesystem::exists(folder.getPath() / "pack.bin"));
    EXPECT_FALSE(std::filesystem::exists(folder.getPath() / "pack.bin.part"));
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

} // namespace haylen::net
