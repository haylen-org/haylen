#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "graphics/TextureResource.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/platform/PluginStreams.hpp"
#include "haylen/platform/VideoStream.hpp"
#include "haylen/plugins/PlatformPlugin.hpp"
#include "support/AllocationTracker.hpp"
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
    haylen::graphics::Device& device = fixture.engine().getGraphics();
    VideoStream stream(VideoStream::Format::Bgra8, 2, 1);
    const haylen::graphics::Texture texture = stream.getTexture(device);
    EXPECT_EQ(texture.getSize(), haylen::math::Vec2(2.0F, 1.0F));
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
    haylen::core::Connection connection = stream.frameReceived.connect([&heard](double timestamp) { heard.push_back(timestamp); });
    EXPECT_TRUE(stream.update(device));
    EXPECT_FALSE(stream.update(device));
    EXPECT_EQ(stream.getFrameCount(), 1U);
    EXPECT_EQ(heard, std::vector<double>{3.0});
    EXPECT_EQ(texture.getResource()->staged, (std::vector<std::uint8_t>{5, 4, 3, 255, 5, 4, 3, 255}));

    // A frame of another size resizes the texture that every handle shares.
    const std::vector<std::byte> larger = makeFrame(3, 2, 9);
    stream.push(larger.data(), 3, 2, getStride(3), 4.0);
    EXPECT_TRUE(stream.update(device));
    EXPECT_EQ(texture.getSize(), haylen::math::Vec2(3.0F, 2.0F));
    EXPECT_EQ(stream.getWidth(), 3);
    EXPECT_EQ(stream.getHeight(), 2);
    EXPECT_EQ(stream.getTimestamp(), 4.0);

    // An engine that stops lets go of the texture and the listeners, and the next one starts from the newest frame.
    stream.detach();
    EXPECT_EQ(stream.frameReceived.size(), 0U);
    const haylen::graphics::Texture again = stream.getTexture(device);
    EXPECT_NE(again, texture);
    EXPECT_EQ(again.getSize(), haylen::math::Vec2(3.0F, 2.0F));
    ASSERT_NE(again.getResource(), nullptr);
    ASSERT_FALSE(again.getResource()->staged.empty());
    EXPECT_EQ(again.getResource()->staged.front(), 11U);

    EXPECT_THROW(stream.push(larger.data(), 0, 2, getStride(3), 0.0), std::invalid_argument);
    EXPECT_THROW(stream.push(larger.data(), 3, 2, 8, 0.0), std::invalid_argument);
    EXPECT_THROW(VideoStream(VideoStream::Format::Rgba8, -1, 0), std::invalid_argument);
}

TEST_F(VideoStreamTest, ReachesTheTextureAtTheStartOfTheNextFrame) {
    test::EngineFixture fixture;
    const std::shared_ptr<VideoStream> stream = PluginStreams::openVideo("stream-tests", "camera", VideoStream::Format::Rgba8, 0, 0);
    fixture.engine().getPlugin<haylen::plugins::PlatformPlugin>().watch(stream);
    const haylen::graphics::Texture texture = stream->getTexture(fixture.engine().getGraphics());
    EXPECT_EQ(texture.getSize(), haylen::math::Vec2(1.0F, 1.0F)) << "A stream without a size shows one transparent pixel.";
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
    EXPECT_EQ(texture.getSize(), haylen::math::Vec2(2.0F, 2.0F));
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().imageWrites, 1);
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().imageWriteBytes, 16U);

    fixture.frames(1);
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().imageWrites, 0);
}

TEST_F(VideoStreamTest, KeepsWatchedStreamsCurrentWithoutAllocating) {
    test::EngineFixture fixture;
    fixture.engine().getPlugin<haylen::plugins::PlatformPlugin>().watch(PluginStreams::openVideo("stream-tests", "allocations", VideoStream::Format::Rgba8, 0, 0));
    fixture.frames(3);
    const test::AllocationTracker tracker;
    fixture.frames(5);
    EXPECT_EQ(tracker.getTotal(), 0U);
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

} // namespace haylen::platform
