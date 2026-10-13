#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "audio/MixerFixture.hpp"
#include "audio/ReaderDecoder.hpp"
#include "audio/SoundData.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/audio/Sound.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "support/AllocationTracker.hpp"
#include "support/CountingPackage.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::audio {

// Plays sounds that stream from a package. Their files hold the position of every frame, so the mix tells exactly which frames of a file a voice played.
class StreamedSoundTest : public MixerFixture {
  protected:
    // Reads a file from memory until a read reaches past an offset, which fails the way a damaged package does.
    class FailingReader final : public io::PackageReader {
      public:
        FailingReader(std::vector<std::uint8_t> file, std::uint64_t limit) : inner(makeReader(std::move(file))), failAt(limit) {}

        [[nodiscard]] std::uint64_t getSize() const noexcept override {
            return inner->getSize();
        }
        [[nodiscard]] std::size_t read(std::uint64_t offset, std::span<std::uint8_t> target) override {
            if (offset + target.size() > failAt) {
                throw std::runtime_error("The record of the file failed its checks.");
            }
            return inner->read(offset, target);
        }

      private:
        std::shared_ptr<io::PackageReader> inner;
        std::uint64_t failAt;
    };

    // Two mebi frames of 16-bit stereo, which make a file of 8 MiB.
    static constexpr std::uint64_t kLongFrames = std::uint64_t{1} << 21;

    [[nodiscard]] static std::shared_ptr<test::CountingPackage> makePackage(std::vector<std::uint8_t> file) {
        std::map<std::string, std::vector<std::uint8_t>> files;
        files.emplace("content/music/long.wav", std::move(file));
        return std::make_shared<test::CountingPackage>(std::make_shared<io::MemoryPackage>("music", std::move(files)));
    }

    [[nodiscard]] static std::vector<std::uint8_t> readTestFile(const std::string& name) {
        std::ifstream stream(std::filesystem::path(HAYLEN_TEST_AUDIO) / name, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    // Tells whether the positions run on one frame at a time from the first, wrapping to the start of a file of the length.
    [[nodiscard]] static bool isContinuous(std::span<const std::uint64_t> positions, std::uint64_t length) {
        for (std::size_t index = 1; index < positions.size(); ++index) {
            if (positions[index] != (positions[index - 1] + 1) % length) {
                ADD_FAILURE() << "Frame " << positions[index] << " followed frame " << positions[index - 1] << ".";
                return false;
            }
        }
        return true;
    }

    // The frames that a voice played from the first frame at the target on, since frames the voice took before a seek may still come first.
    [[nodiscard]] static std::span<const std::uint64_t> from(const std::vector<std::uint64_t>& positions, std::uint64_t target) {
        const auto first = std::ranges::find(positions, target);
        return {first, positions.end()};
    }
};

TEST_F(StreamedSoundTest, PlaysSeeksAndLoopsAFileOfSeveralMegabytesInBoundedMemory) {
    const std::vector<std::uint8_t> file = makePositions(kLongFrames);
    const std::shared_ptr<test::CountingPackage> package = makePackage(file);
    const test::AllocationTracker tracker;
    const Sound sound = Sound::stream(package->openAssetReader("music/long.wav"));
    EXPECT_TRUE(sound.isStreamed());
    EXPECT_EQ(sound.getFrameCount(), kLongFrames);
    EXPECT_EQ(sound.getChannels(), 2U);
    EXPECT_TRUE(sound.getData()->samples.empty());

    // This thread mixes, as the audio thread does, and never reads the package.
    package->watch(std::this_thread::get_id());
    const Mixer::VoiceId voice = mixer.play(sound, {.bus = "music", .loop = true});
    const std::vector<std::uint64_t> start = listen(voice, kRate / 2);
    ASSERT_EQ(start.size(), kRate / 2);
    EXPECT_EQ(start.front(), 0U);
    EXPECT_TRUE(isContinuous(start, kLongFrames));

    // A seek plays on from the new cursor.
    mixer.setCursor(voice, 30.0F);
    const std::vector<std::uint64_t> middle = listen(voice, kRate / 2 + 4096);
    const std::span<const std::uint64_t> seeked = from(middle, 30 * kRate);
    EXPECT_GE(seeked.size(), kRate / 2);
    EXPECT_TRUE(isContinuous(seeked, kLongFrames));
    EXPECT_NEAR(mixer.getCursor(voice), 30.5F, 0.2F);

    // A looping voice runs from the last frame of the file into the first.
    const float nearEnd = static_cast<float>(kLongFrames - kRate / 4) / static_cast<float>(kRate);
    mixer.setCursor(voice, nearEnd);
    const std::vector<std::uint64_t> end = listen(voice, kRate / 2 + 4096);
    const std::span<const std::uint64_t> looped = from(end, static_cast<std::uint64_t>(nearEnd * static_cast<float>(kRate)));
    EXPECT_GE(looped.size(), kRate / 2);
    EXPECT_TRUE(isContinuous(looped, kLongFrames));
    EXPECT_TRUE(std::ranges::find(looped, 0U) != looped.end());
    EXPECT_LT(mixer.getCursor(voice), 1.0F);

    EXPECT_LT(tracker.getLargest(), file.size() / 8) << "No allocation comes near the size of the file.";
    EXPECT_LE(package->getLargestRead(), ReaderDecoder::kWindowSize);
    EXPECT_LT(package->getReadBytes(), file.size() / 4);
    EXPECT_GT(package->getReadBytes(), 0U);
    EXPECT_EQ(package->getWatchedReads(), 0U) << "The thread that mixes never reads the package.";
}

TEST_F(StreamedSoundTest, PlaysTheFramesOfItsDecodedSoundAndEndsWithThem) {
    const std::vector<std::uint8_t> file = makePositions(kRate / 4);
    const Sound decoded = Sound::decode(file);
    const Sound streamed = Sound::stream(makeReader(file));

    const std::vector<std::uint64_t> expected = listen(mixer.play(decoded), kRate);
    const Mixer::VoiceId voice = mixer.play(streamed);
    const std::vector<std::uint64_t> played = listen(voice, kRate);
    EXPECT_EQ(played, expected);
    EXPECT_EQ(played.front(), 0U);
    EXPECT_TRUE(isContinuous(played, kRate / 4));
    EXPECT_FALSE(mixer.isActive(voice));

    // A voice that starts later in the file plays only the rest of it.
    const std::vector<std::uint64_t> expectedRest = listen(mixer.play(decoded, {.startAt = 0.125F}), kRate);
    const Mixer::VoiceId late = mixer.play(streamed, {.startAt = 0.125F});
    const std::vector<std::uint64_t> rest = listen(late, kRate);
    EXPECT_EQ(from(rest, kRate / 8).size(), expectedRest.size());
    EXPECT_EQ(expectedRest.front(), kRate / 8);
    EXPECT_FALSE(mixer.isActive(late));
}

TEST_F(StreamedSoundTest, EndsAVoiceWhoseReadsFail) {
    const Sound sound = Sound::stream(std::make_shared<FailingReader>(makePositions(4 * kRate), 256 * 1024));
    const Mixer::VoiceId voice = mixer.play(sound);
    const std::vector<std::uint64_t> played = listen(voice, 4 * kRate);
    EXPECT_FALSE(mixer.isActive(voice));
    EXPECT_GT(played.size(), 0U);
    EXPECT_LT(played.size(), 4 * kRate);
    EXPECT_TRUE(isContinuous(played, 4 * kRate));
    ASSERT_EQ(jobErrors.size(), 1U);
    EXPECT_EQ(jobErrors.front(), "The record of the file failed its checks.");
    jobErrors.clear();
}

TEST_F(StreamedSoundTest, DecodesThroughAWindowOfTheFile) {
    const std::shared_ptr<test::CountingPackage> package = makePackage(makePositions(kLongFrames));
    const std::shared_ptr<io::PackageReader> reader = package->openAssetReader("music/long.wav");

    // A standard library may give a large block room of its own, as MSVC does to align it, so the bound is the allocation of a buffer of one window.
    // clang-format off
    const std::size_t windowAllocation = [] {
        const test::AllocationTracker probe;
        const std::vector<std::uint8_t> window(ReaderDecoder::kWindowSize);
        return probe.getLargest();
    }();
    // clang-format on
    const test::AllocationTracker tracker;
    ReaderDecoder decoder(reader);
    EXPECT_EQ(decoder.getLength(), kLongFrames);

    std::vector<float> block(4096 * 2);
    std::uint64_t frames = 0;
    for (std::uint64_t count = decoder.read(block); count > 0; count = decoder.read(block)) {
        frames += count;
    }
    EXPECT_EQ(frames, kLongFrames);

    // A seek reads only the bytes of the frame it moves to.
    package->reset();
    EXPECT_TRUE(decoder.seek(kLongFrames / 2));
    ASSERT_EQ(decoder.read(std::span(block).first(2)), 1U);
    EXPECT_EQ(std::lround(block[0] * 32768.0F) - 1, static_cast<long>((kLongFrames / 2) % kPositionPeriod));
    EXPECT_LE(package->getReadBytes(), ReaderDecoder::kWindowSize);

    EXPECT_LE(tracker.getLargest(), windowAllocation);
    EXPECT_LE(package->getLargestRead(), ReaderDecoder::kWindowSize);
}

TEST_F(StreamedSoundTest, StreamsEveryCompressedFormatLikeItsDecodedSound) {
    for (const std::string name : {"tone.ogg", "tone.mp3", "tone_without_header.mp3", "tone.flac"}) {
        SCOPED_TRACE(name);
        const std::vector<std::uint8_t> file = readTestFile(name);
        ASSERT_FALSE(file.empty());
        const Sound decoded = Sound::decode(file);
        const Sound streamed = Sound::stream(makeReader(file));
        EXPECT_EQ(streamed.getFrameCount(), decoded.getFrameCount());
        EXPECT_EQ(streamed.getChannels(), decoded.getChannels());
        EXPECT_EQ(streamed.getSampleRate(), decoded.getSampleRate());

        ReaderDecoder decoder(makeReader(file));
        std::vector<float> samples(decoded.getData()->samples.size() + 1024);
        const std::uint64_t frames = decoder.read(samples);
        samples.resize(frames * decoder.getChannels());
        EXPECT_EQ(samples, decoded.getData()->samples);
    }
}

TEST_F(StreamedSoundTest, ReadsTheLengthOfAnOggFileFromItsLastWholePage) {
    const std::vector<std::uint8_t> file = readTestFile("tone.ogg");
    const std::uint64_t whole = Sound::stream(makeReader(file)).getFrameCount();
    EXPECT_GT(whole, 0U);

    // A file cut short reports the frames up to the last page it still holds whole, or none, and never fails.
    for (const std::size_t cut : {std::size_t{1}, std::size_t{20}, std::size_t{400}}) {
        const std::vector<std::uint8_t> shorter(file.begin(), file.end() - static_cast<std::ptrdiff_t>(cut));
        EXPECT_LE(Sound::stream(makeReader(shorter)).getFrameCount(), whole) << cut;
    }
    std::vector<std::uint8_t> garbage = file;
    std::fill(garbage.end() - 64, garbage.end(), std::uint8_t{'S'});
    EXPECT_LE(Sound::stream(makeReader(garbage)).getFrameCount(), whole);
}

TEST_F(StreamedSoundTest, LoadsAStreamedAssetWithoutReadingItsFile) {
    const std::vector<std::uint8_t> file = makePositions(kLongFrames);
    test::EngineFixture fixture({{"content/music/long.wav", std::string(file.begin(), file.end())}});
    assets::Manager& assets = fixture.engine().getAssets();

    // An asynchronous load opens the reader and reads the header on the I/O pool.
    std::shared_ptr<void> asset;
    // clang-format off
    assets.loadAsync("sound", "music/long.wav", [&asset](std::shared_ptr<void> streamed, const std::string& error) {
        EXPECT_EQ(error, "");
        asset = std::move(streamed);
    }, {{"stream", true}});
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&asset] { return asset != nullptr; }));
    EXPECT_TRUE(audio::Sound(std::static_pointer_cast<const SoundData>(asset)).isStreamed());

    // A synchronous load does the same on the frame thread.
    asset.reset();
    {
        const test::AllocationTracker tracker;
        asset = assets.load("sound", "music/long.wav", {{"stream", true}});
        EXPECT_LT(tracker.getLargest(), file.size() / 8);
    }
    const Sound sound(std::static_pointer_cast<const SoundData>(asset));
    EXPECT_TRUE(sound.isStreamed());
    EXPECT_EQ(sound.getFrameCount(), kLongFrames);
}

} // namespace haylen::audio
