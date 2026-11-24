#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "haylen/platform/PluginStreams.hpp"
#include "haylen/platform/VideoStream.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::platform {

TEST(StreamsLuaTest, HandsPluginModulesTheStreamsOfTheirNativeParts) {
    test::LuaFixture fixture({{"haylen.json", R"({"name": "Test App", "identifier": "com.haylen.tests", "plugins": {"cam-kit": {}}})"}, {"plugins/cam-kit/plugin.json", R"({"id": "cam-kit", "version": "1.0.0", "implementations": {"ios": "lua"}})"}});
    fixture.runLua("platform = require('haylen.platform') collections = require('haylen.collections') audio = require('haylen.audio') cam = platform.plugin('cam-kit')");
    EXPECT_EQ(fixture.lua("return tostring(cam:videoStream('preview')) .. ' ' .. tostring(cam:audioStream('mic'))"), "nil nil");

    const std::shared_ptr<haylen::platform::VideoStream> video = haylen::platform::PluginStreams::openVideo("cam-kit", "preview", haylen::platform::VideoStream::Format::Rgba8, 2, 1);
    const std::shared_ptr<haylen::platform::AudioStream> mic = haylen::platform::PluginStreams::openAudio("cam-kit", "mic", 16000, 2, haylen::platform::AudioStream::Format::Float32, 1600);
    // clang-format off
    fixture.runLua(R"(
        preview = cam:videoStream('preview')
        frames = {}
        preview:on('frame', function(timestamp) frames[#frames + 1] = timestamp end)
        texture = preview.texture
        mic = cam:audioStream('mic')
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return table.concat({preview.width, preview.height, preview.frameCount, texture.width, getmetatable(preview), getmetatable(mic), tostring(preview == cam:videoStream('preview'))}, ' ')"), "2 1 0 2 haylen.VideoStream haylen.AudioStream true");

    // clang-format off
    std::thread producer([&video] {
        const std::vector<std::byte> frame(4 * 4 * 4, std::byte{0x40});
        video->push(frame.data(), 4, 4, 16, 1.5);
    });
    // clang-format on
    producer.join();
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat({#frames, frames[1], preview.width, preview.height, texture.width, preview.frameCount, preview.timestamp}, ' ')"), "1 1.5 4 4 4 1 1.5");

    // The app reads the newest samples for analysis and plays the stream as a voice with the options that apply to it.
    EXPECT_EQ(mic->push(std::vector<float>{0.25F, -0.25F, 0.5F, -0.5F}), 2U);
    EXPECT_EQ(fixture.lua("buffer = collections.newFloatBuffer(6) return table.concat({mic:read(buffer), buffer[1], buffer[4], buffer[5], mic.sampleRate, mic.channels, mic.underruns}, ' ')"), "4 0.25 -0.5 0.0 16000 2 0");
    EXPECT_EQ(fixture.lua("voice = mic:play({bus = 'music', volume = 0.5, fadeIn = 0.1}) return tostring(audio.active(voice))"), "true");
    EXPECT_NE(fixture.lua("mic:play({loop = true})").find("loop"), std::string::npos);
    EXPECT_NE(fixture.lua("preview:on('ended', print)").find("Unknown video stream event \"ended\". Video streams report \"frame\"."), std::string::npos);

    // An app that stops lets go of the texture and the listeners of the streams it drew.
    fixture.restart();
    EXPECT_EQ(video->frameReceived.size(), 0U);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

} // namespace haylen::lua::platform
