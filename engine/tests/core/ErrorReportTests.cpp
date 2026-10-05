#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <string_view>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/lua/Error.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::core {

// One way app code fails: the module `fault` of a recoverable app that raises from one kind of code, the message the error screen shows first and the line of the module it points at.
struct FailingCode {
    std::string_view name;
    std::string_view module;
    std::string_view message;
    int line = 0;
};

// Every kind of code that runs on behalf of the app reports its error the same way: the error screen, the host, which hands it to the web page, and `appError` get the message, the file, the line and a stack through the failing line, and the app runs on after `haylen.recover()`.
class ErrorReportTest : public ::testing::TestWithParam<FailingCode> {
  protected:
    static constexpr std::string_view kMain = R"(local events = require('haylen.events')
local haylen = require('haylen')
local scene = require('haylen.scene')

local Counter = {update = function() frames = frames + 1 end}
frames, recovered = 0, 0
haylen.setRecoverable(true)
events.on('appError', function(error) heard = error.message end)
events.on('appRecovered', function()
    recovered = recovered + 1
    scene.clear()
    scene.push(Counter)
end)
scene.push(Counter)
require('fault')
)";
};

TEST_P(ErrorReportTest, ShowsTheErrorWithItsStackAndRecovers) {
    const FailingCode& code = GetParam();
    test::EngineFixture fixture({{"source/main.lua", std::string(kMain)}, {"source/fault.lua", std::string(code.module)}, {"content/broken.png", "not an image"}});
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.engine().getError() != nullptr; })) << code.name;

    const lua::Error& error = *fixture.engine().getError();
    EXPECT_TRUE(error.getMessage().starts_with(code.message)) << error.getMessage();
    EXPECT_EQ(error.getFile(), "source/fault.lua");
    EXPECT_EQ(error.getLine(), code.line);
    EXPECT_TRUE(std::ranges::any_of(error.getFrames(), [&](const lua::Error::Frame& frame) { return frame.source == "source/fault.lua" && frame.line == code.line; })) << error.getTraceback();
    ASSERT_EQ(fixture.host().getErrorReports().size(), 1U);
    EXPECT_EQ(fixture.host().getErrorReports().front(), error.toJson());
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return heard"), error.getMessage());

    // The app goes back to its scenes with the same Lua state and keeps updating.
    const std::string before = fixture.lua("return frames");
    fixture.runLua("require('haylen').recover()");
    fixture.frames(3);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(fixture.lua("return recovered"), "1");
    EXPECT_NE(fixture.lua("return frames"), before);
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(Callbacks, ErrorReportTest, ::testing::Values(
    FailingCode{"sceneUpdate", R"(local scene = require('haylen.scene')
scene.push({update = function()
    error('The update failed.')
end})
)", "The update failed.", 3},
    FailingCode{"sceneEnter", R"(local scene = require('haylen.scene')
scene.push({enter = function()
    error('The enter hook failed.')
end})
)", "The enter hook failed.", 3},
    FailingCode{"asyncTask", R"(local async = require('async')
async.spawn(function()
    async.sleep(1):await()
    error('The task failed after it waited.')
end)
)", "The task failed after it waited.", 4},
    FailingCode{"ownedTask", R"(local async = require('async')
local scene = require('haylen.scene')
owner = {}
scene.spawn(owner, function()
    async.sleep(1):await()
    error('The owned task failed.')
end)
)", "The owned task failed.", 6},
    FailingCode{"timer", R"(local timer = require('haylen.timer')
timer.after(0.01, function()
    error('The timer failed.')
end)
)", "The timer failed.", 3},
    FailingCode{"tween", R"(local tween = require('haylen.tween')
target = {x = 0}
tween.to(target, 0.02, {x = 1}, {onComplete = function()
    error('The tween failed.')
end})
)", "The tween failed.", 4},
    FailingCode{"signal", R"(local signal = require('haylen.signal')
local timer = require('haylen.timer')
local changed = signal.new('fault.changed')
changed:connect(function()
    error('The signal handler failed.')
end)
timer.after(0.01, function() changed:emit() end)
)", "The signal handler failed.", 5},
    FailingCode{"queuedEvent", R"(local events = require('haylen.events')
events.on('fault.happened', function()
    error('The event listener failed.')
end)
events.post('fault.happened')
)", "The event listener failed.", 3},
    FailingCode{"nativeEvent", R"(local platform = require('haylen.platform')
platform.on('fault.native', function()
    error('The native event listener failed.')
end)
platform.emit('fault.native', {})
)", "The native event listener failed.", 3},
    FailingCode{"platformAnswer", R"(local async = require('async')
local platform = require('haylen.platform')
platform.registerHandler('fault.answer', function() return {value = 7} end)
async.spawn(function()
    local answer = platform.call('fault.answer'):await()
    error('The answer was ' .. answer.value .. '.')
end)
)", "The answer was 7.", 6},
    FailingCode{"assetLoad", R"(local assets = require('haylen.assets')
local async = require('async')
async.spawn(function()
    local _, failure = assets.loadAsync('broken.png', 'texture'):await()
    error(failure, 0)
end)
)", "The image could not be decoded", 5},
    FailingCode{"socketListener", R"(local net = require('haylen.net')
local socket = net.connectWebSocket('ws://127.0.0.1:1/')
socket:on('error', function()
    error('The socket listener failed.')
end)
)", "The socket listener failed.", 4}),
    [](const ::testing::TestParamInfo<FailingCode>& info) { return std::string(info.param.name); });
// clang-format on

} // namespace haylen::core
