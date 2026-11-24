#include <gtest/gtest.h>

#include <cstddef>
#include <string>

#include "haylen/core/MessagePack.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::core {

TEST(MessagePackLuaTest, EncodesAndDecodesOnTheFrameThreadAndOffIt) {
    test::LuaFixture fixture;
    fixture.runLua("net = require('haylen.net') platform = require('haylen.platform')");
    EXPECT_EQ(fixture.lua("local value = net.decodeMessagePack(net.encodeMessagePack({o = 30, t = 12, name = 'stone', list = {1, 2, 3}, raw = platform.bytes('\\0\\1\\2')})) return value.o .. ' ' .. value.t .. ' ' .. value.name .. ' ' .. #value.list .. ' ' .. #value.raw .. ' ' .. value.raw:byte(3)"), "30 12 stone 3 3 2");

    // clang-format off
    fixture.runLua(R"(
        require('async').spawn(function()
            local frames = {}
            for index = 1, 2000 do
                frames[index] = {o = index, tiles = {1, 2, 3}}
            end
            local value = net.decodeMessagePackAsync(net.encodeMessagePack({o = 5, f = frames})):await()
            decoded = value.o .. ' ' .. #value.f .. ' ' .. value.f[2000].o
            local _, failure = net.decodeMessagePackAsync('\193'):await()
            refused = failure
        end)
    )");
    // clang-format on

    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return tostring(refused ~= nil)") == "true"; }));
    EXPECT_EQ(fixture.lua("return decoded"), "5 2000 2000");
    EXPECT_EQ(fixture.lua("return refused"), "The bytes are not valid MessagePack: it holds the byte 0xc1, which no value uses.");
    EXPECT_NE(fixture.lua("net.decodeMessagePack('\\147\\1')").find("The bytes are not valid MessagePack: a container claims more items than its bytes hold."), std::string::npos);
    EXPECT_NE(fixture.lua("net.encodeMessagePack(print)").find("cannot be converted to JSON"), std::string::npos);
}

} // namespace haylen::lua::core
