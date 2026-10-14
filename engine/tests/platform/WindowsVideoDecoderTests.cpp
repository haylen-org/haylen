#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

#include "io/FileReader.hpp"
#include "platform/windows/WindowsVideoDecoder.hpp"

namespace haylen::platform {

namespace {

class WindowsVideoDecoderTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::shared_ptr<io::PackageReader> open(const char* name) {
        return std::make_shared<io::FileReader>(std::filesystem::path(HAYLEN_TEST_VIDEO) / name, name);
    }
};

} // namespace

// The clip made for the tests plays through Media Foundation: its size and length, pictures in order as RGBA rows, the sound and a seek.
TEST_F(WindowsVideoDecoderTest, DecodesPicturesAndSoundOfAClip) {
    WindowsVideoDecoder decoder(open("clip.mp4"), "clip.mp4");
    const VideoDecoder::Info& info = decoder.getInfo();
    ASSERT_GT(info.width, 0);
    ASSERT_GT(info.height, 0);
    EXPECT_GT(info.duration, 0.0);

    VideoDecoder::Frame frame;
    double last = -1.0;
    for (int index = 0; index < 5; ++index) {
        ASSERT_EQ(decoder.readFrame(frame), VideoDecoder::Read::Frame);
        EXPECT_EQ(frame.width, info.width);
        EXPECT_EQ(frame.pixels.size(), static_cast<std::size_t>(info.width) * static_cast<std::size_t>(info.height) * 4U);
        EXPECT_GT(frame.time, last);
        last = frame.time;
    }
    if (info.channels > 0) {
        std::vector<float> samples;
        decoder.readAudio(samples, 0.5);
        EXPECT_FALSE(samples.empty());
    }

    decoder.seek(info.duration * 0.5);
    ASSERT_EQ(decoder.readFrame(frame), VideoDecoder::Read::Frame);
    EXPECT_GE(frame.time, info.duration * 0.5 - 0.5);
    while (decoder.readFrame(frame) == VideoDecoder::Read::Frame) {}
    EXPECT_EQ(decoder.readFrame(frame), VideoDecoder::Read::End);
}

TEST_F(WindowsVideoDecoderTest, RefusesFilesThatAreNoVideo) {
    EXPECT_THROW(WindowsVideoDecoder(std::make_shared<io::FileReader>(std::filesystem::path(HAYLEN_TEST_AUDIO) / "tone.ogg", "tone.ogg"), "tone.ogg"), std::runtime_error);
}

} // namespace haylen::platform
