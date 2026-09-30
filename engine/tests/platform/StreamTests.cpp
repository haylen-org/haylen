#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "graphics/TextureResource.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/platform/PluginStreams.hpp"
#include "haylen/platform/VideoStream.hpp"
#include "plugins/PlatformPlugin.hpp"
#include "sokol_gfx.h"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

class VideoStreamTest : public ::testing::Test {
  protected:
    // A frame of `width` by `height` pixels whose bytes are the seed, the seed plus 1 and plus 2 and an opaque alpha, with four bytes of padding after every row.
    [[nodiscard]] static std::vector<std::byte> makeFrame(int width, int height, std::uint8_t seed) {
        std::vector<std::byte> frame(getStride(width) * static_cast<std::size_t>(height), std::byte{0xEE});
        for (std::size_t row = 0; row < static_cast<std::size_t>(height); ++row) {
            for (std::size_t column = 0; column < static_cast<std::size_t>(width); ++column) {
                std::byte* pixel = frame.data() + row * getStride(width) + column * 4;
                pixel[0] = std::byte{seed};
                pixel[1] = std::byte{static_cast<std::uint8_t>(seed + 1)};
                pixel[2] = std::byte{static_cast<std::uint8_t>(seed + 2)};
                pixel[3] = std::byte{255};
            }
        }
        return frame;
    }

    [[nodiscard]] static std::size_t getStride(int width) {
        return static_cast<std::size_t>(width) * 4 + 4;
    }
};

TEST_F(VideoStreamTest, UploadsOnlyTheNewestFrameAndResizesInPlace) {
    test::EngineFixture fixture;
    graphics::Device& device = fixture.engine().getGraphics();
    VideoStream stream(VideoStream::Format::Bgra8, 2, 1);
    const graphics::Texture texture = stream.getTexture(device);
    EXPECT_EQ(texture.getSize(), math::Vec2(2.0F, 1.0F));
    EXPECT_EQ(stream.getWidth(), 2);
    EXPECT_FALSE(stream.update(device)) << "No frame arrived yet.";

    // Three frames arrive from another thread before the engine looks, and only the newest one reaches the texture, turned from BGRA into RGBA without its padding.
    // clang-format off
    std::thread producer([&stream] {
        for (std::uint8_t seed = 1; seed <= 3; ++seed) {
            const std::vector<std::byte> frame = makeFrame(2, 1, seed);
            stream.push(frame.data(), 2, 1, getStride(2), seed);
        }
    });
    // clang-format on
    producer.join();
    std::vector<double> heard;
    core::Connection connection = stream.frameReceived.connect([&heard](double timestamp) { heard.push_back(timestamp); });
    EXPECT_TRUE(stream.update(device));
    EXPECT_FALSE(stream.update(device));
    EXPECT_EQ(stream.getFrameCount(), 1U);
    EXPECT_EQ(heard, std::vector<double>{3.0});
    EXPECT_EQ(texture.getResource()->staged, (std::vector<std::uint8_t>{5, 4, 3, 255, 5, 4, 3, 255}));

    // A frame of another size resizes the texture that every handle shares.
    const std::vector<std::byte> larger = makeFrame(3, 2, 9);
    stream.push(larger.data(), 3, 2, getStride(3), 4.0);
    EXPECT_TRUE(stream.update(device));
    EXPECT_EQ(texture.getSize(), math::Vec2(3.0F, 2.0F));
    EXPECT_EQ(stream.getWidth(), 3);
    EXPECT_EQ(stream.getHeight(), 2);
    EXPECT_EQ(stream.getTimestamp(), 4.0);

    // An engine that stops lets go of the texture and the listeners, and the next one starts from the newest frame.
    stream.detach();
    EXPECT_EQ(stream.frameReceived.size(), 0U);
    const graphics::Texture again = stream.getTexture(device);
    EXPECT_NE(again, texture);
    EXPECT_EQ(again.getSize(), math::Vec2(3.0F, 2.0F));
    EXPECT_EQ(again.getResource()->staged.front(), 11U);

    EXPECT_THROW(stream.push(larger.data(), 0, 2, getStride(3), 0.0), std::invalid_argument);
    EXPECT_THROW(stream.push(larger.data(), 3, 2, 8, 0.0), std::invalid_argument);
    EXPECT_THROW(VideoStream(VideoStream::Format::Rgba8, -1, 0), std::invalid_argument);
}

TEST_F(VideoStreamTest, ReachesTheTextureAtTheStartOfTheNextFrame) {
    test::EngineFixture fixture;
    const std::shared_ptr<VideoStream> stream = PluginStreams::openVideo("stream-tests", "camera", VideoStream::Format::Rgba8, 0, 0);
    fixture.engine().getPlugin<plugins::PlatformPlugin>().watch(stream);
    const graphics::Texture texture = stream->getTexture(fixture.engine().getGraphics());
    EXPECT_EQ(texture.getSize(), math::Vec2(1.0F, 1.0F)) << "A stream without a size shows one transparent pixel.";
    fixture.frames(1);

    // clang-format off
    std::thread producer([&stream] {
        for (std::uint8_t seed = 1; seed <= 3; ++seed) {
            const std::vector<std::byte> frame = makeFrame(2, 2, seed);
            stream->push(frame.data(), 2, 2, getStride(2), seed);
        }
    });
    // clang-format on
    producer.join();
    EXPECT_EQ(stream->getFrameCount(), 0U);
    fixture.frames(1);
    EXPECT_EQ(stream->getFrameCount(), 1U);
    EXPECT_EQ(texture.getSize(), math::Vec2(2.0F, 2.0F));
    EXPECT_EQ(sg_query_stats().prev_frame.num_update_image, 1U);
    EXPECT_EQ(sg_query_stats().prev_frame.size_update_image, 16U);

    fixture.frames(1);
    EXPECT_EQ(sg_query_stats().prev_frame.num_update_image, 0U);
}

TEST(PluginStreamsTest, OpensEachStreamOnceByPluginAndName) {
    const std::shared_ptr<VideoStream> video = PluginStreams::openVideo("stream-tests", "registry", VideoStream::Format::Rgba8, 4, 4);
    EXPECT_EQ(PluginStreams::openVideo("stream-tests", "registry", VideoStream::Format::Rgba8, 8, 8), video);
    EXPECT_EQ(PluginStreams::findVideo("stream-tests", "registry"), video);
    EXPECT_EQ(video->getWidth(), 4);
    EXPECT_EQ(PluginStreams::findVideo("stream-tests", "elsewhere"), nullptr);
    EXPECT_THROW((void)PluginStreams::openVideo("stream-tests", "registry", VideoStream::Format::Bgra8, 4, 4), std::invalid_argument);
    EXPECT_THROW((void)PluginStreams::openVideo("", "registry", VideoStream::Format::Rgba8, 4, 4), std::invalid_argument);

    const std::shared_ptr<AudioStream> audio = PluginStreams::openAudio("stream-tests", "registry", 16000, 1, AudioStream::Format::Int16, 1600);
    EXPECT_EQ(PluginStreams::openAudio("stream-tests", "registry", 16000, 1, AudioStream::Format::Int16, 3200), audio);
    EXPECT_EQ(PluginStreams::findAudio("stream-tests", "registry"), audio);
    EXPECT_EQ(audio->getCapacity(), 1600U);
    EXPECT_THROW((void)PluginStreams::openAudio("stream-tests", "registry", 16000, 2, AudioStream::Format::Int16, 1600), std::invalid_argument);
    EXPECT_THROW((void)PluginStreams::openAudio("stream-tests", "", 16000, 1, AudioStream::Format::Int16, 1600), std::invalid_argument);
    EXPECT_THROW((void)PluginStreams::openAudio("stream-tests", "silent", 16000, 1, AudioStream::Format::Int16, 0), std::invalid_argument);
}

TEST(StreamsLuaTest, HandsPluginModulesTheStreamsOfTheirNativeParts) {
    test::EngineFixture fixture({{"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "plugins": {"cam-kit": {}}})"}, {"plugins/cam-kit/plugin.json", R"({"id": "cam-kit", "version": "1.0.0"})"}});
    fixture.runLua("platform = require('haylen.platform') collections = require('haylen.collections') audio = require('haylen.audio') cam = platform.plugin('cam-kit')");
    EXPECT_EQ(fixture.lua("return tostring(cam:videoStream('preview')) .. ' ' .. tostring(cam:audioStream('mic'))"), "nil nil");

    const std::shared_ptr<VideoStream> video = PluginStreams::openVideo("cam-kit", "preview", VideoStream::Format::Rgba8, 2, 1);
    const std::shared_ptr<AudioStream> mic = PluginStreams::openAudio("cam-kit", "mic", 16000, 2, AudioStream::Format::Float32, 1600);
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

} // namespace haylen::platform
