#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/MessagePack.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::core {

class MessagePackTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::vector<std::byte> bytesOf(std::initializer_list<int> values) {
        std::vector<std::byte> bytes;
        for (const int value : values) {
            bytes.push_back(static_cast<std::byte>(value));
        }

        return bytes;
    }

    [[nodiscard]] static std::string refusal(const std::vector<std::byte>& bytes) {
        try {
            (void)haylen::core::MessagePack::decode(bytes);
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return {};
    }
};

TEST_F(MessagePackTest, RoundTripsEveryKindOfValue) {
    const haylen::core::Json value = {{"o", 30}, {"negative", -5}, {"small", -33}, {"wide", 4294967296LL}, {"float", 1.5}, {"text", "héllo"}, {"list", {1, nullptr, true, false}}, {"nested", {{"a", {{"b", haylen::core::Json::array()}}}}}, {"raw", haylen::core::Json::binary({0, 255, 7})}};
    const std::vector<std::byte> encoded = haylen::core::MessagePack::encode(value);
    EXPECT_EQ(haylen::core::MessagePack::decode(encoded), value);

    // Known encodings: a positive and a negative fixed integer, a fixed map with a fixed string key and an unsigned 16-bit integer.
    EXPECT_EQ(haylen::core::MessagePack::decode(bytesOf({0x07})), 7);
    EXPECT_EQ(haylen::core::MessagePack::decode(bytesOf({0xFF})), -1);
    EXPECT_EQ(haylen::core::MessagePack::decode(bytesOf({0x81, 0xA1, 'o', 0xCD, 0x01, 0x00})), (haylen::core::Json{{"o", 256}}));
    EXPECT_EQ(haylen::core::MessagePack::encode(haylen::core::Json(7)), bytesOf({0x07}));
}

TEST_F(MessagePackTest, RefusesBrokenAndHostileMessages) {
    EXPECT_EQ(refusal({}), "The bytes are not valid MessagePack: it ends before its last value.");
    EXPECT_EQ(refusal(bytesOf({0x07, 0x07})), "The bytes are not valid MessagePack: bytes follow its value.");
    EXPECT_EQ(refusal(bytesOf({0x92, 0x01})), "The bytes are not valid MessagePack: a container claims more items than its bytes hold.");
    EXPECT_EQ(refusal(bytesOf({0xA5, 'a'})), "The bytes are not valid MessagePack: a value runs past its end.");
    EXPECT_EQ(refusal(bytesOf({0xD4, 0x01, 0x02})), "The bytes are not valid MessagePack: it uses extension types, which the engine does not read.");
    EXPECT_EQ(refusal(bytesOf({0xC1})), "The bytes are not valid MessagePack: it holds the byte 0xc1, which no value uses.");
    EXPECT_EQ(refusal(bytesOf({0xDD, 0xFF, 0xFF, 0xFF, 0xFF, 0x01})), "The bytes are not valid MessagePack: a container claims more items than its bytes hold.");
    EXPECT_EQ(refusal(bytesOf({0x81, 0x01, 0x02})).find("The bytes are not valid MessagePack: "), 0U) << "Keys that are not strings have no JSON form.";

    // A million nested arrays never reach the recursion of the decoder.
    std::vector<std::byte> deep(1000000, std::byte{0x91});
    deep.push_back(std::byte{0x00});
    EXPECT_EQ(refusal(deep), "The bytes are not valid MessagePack: it nests deeper than 128 levels.");

    haylen::core::Json nested = haylen::core::Json::array();
    for (int level = 0; level < 200; ++level) {
        nested = haylen::core::Json::array({nested});
    }

    EXPECT_THROW((void)haylen::core::MessagePack::encode(nested), std::invalid_argument);

    // Both ways count only containers as levels, so 128 nested arrays round-trip and a 129th one fails both ways, even when it is empty.
    haylen::core::Json deepest = haylen::core::Json::array({0});
    for (int level = 1; level < 128; ++level) {
        deepest = haylen::core::Json::array({deepest});
    }

    EXPECT_EQ(haylen::core::MessagePack::decode(haylen::core::MessagePack::encode(deepest)), deepest);
    EXPECT_THROW((void)haylen::core::MessagePack::encode(haylen::core::Json::array({deepest})), std::invalid_argument);

    std::vector<std::byte> empty(128, std::byte{0x91});
    empty.push_back(std::byte{0x90});
    EXPECT_EQ(refusal(empty), "The bytes are not valid MessagePack: it nests deeper than 128 levels.");
}

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
