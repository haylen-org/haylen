#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
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

    // Reads a file from memory and keeps the lowest offset that a read started at since the last reset.
    class LowestReadReader final : public io::PackageReader {
      public:
        explicit LowestReadReader(std::vector<std::uint8_t> file) : inner(makeReader(std::move(file))) {}

        [[nodiscard]] std::uint64_t getSize() const noexcept override {
            return inner->getSize();
        }
        [[nodiscard]] std::size_t read(std::uint64_t offset, std::span<std::uint8_t> target) override {
            lowest = std::min(lowest, offset);
            return inner->read(offset, target);
        }
        [[nodiscard]] std::uint64_t getLowest() const noexcept {
            return lowest;
        }
        void reset() noexcept {
            lowest = std::numeric_limits<std::uint64_t>::max();
        }

      private:
        std::shared_ptr<io::PackageReader> inner;
        std::uint64_t lowest = std::numeric_limits<std::uint64_t>::max();
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

TEST_F(StreamedSoundTest, LoopsBetweenLoopPointsWithoutAGap) {
    const std::vector<std::uint8_t> file = makePositions(kRate);
    for (const bool streamed : {false, true}) {
        const Sound sound = streamed ? Sound::stream(makeReader(file)) : Sound::decode(file);
        const Mixer::VoiceId voice = mixer.play(sound, {.loop = true, .loopStart = 0.25F, .loopEnd = 0.5F});
        const std::vector<std::uint64_t> played = listen(voice, kRate);
        ASSERT_EQ(played.size(), kRate) << streamed;

        // The intro plays once, and every frame after the loop end comes from the loop start on.
        std::uint64_t expected = 0;
        for (const std::uint64_t position : played) {
            ASSERT_EQ(position, expected) << streamed;
            expected = expected + 1 == kRate / 2 ? kRate / 4 : expected + 1;
        }
        mixer.stop(voice);
        peak(toFrames(Mixer::kDeclickSeconds) + 1024);
        mixer.update(0.0F);
    }
    EXPECT_THROW((void)mixer.play(makeTone(100), {.loop = true, .loopStart = 0.5F}), std::invalid_argument);
    EXPECT_THROW((void)mixer.play(makeTone(toFrames(1.0F)), {.loop = true, .loopStart = 0.5F, .loopEnd = 0.4F}), std::invalid_argument);

    // A loop shorter than a frame would repeat nothing, which would keep a decoder busy forever.
    try {
        (void)mixer.play(makeTone(toFrames(1.0F)), {.loop = true, .loopStart = 0.5F, .loopEnd = 0.500001F});
        FAIL() << "A loop shorter than a frame plays.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "A loop needs to span at least one frame of the sound.");
    }
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
    ASSERT_TRUE(awaitJobErrors(1));
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
    for (const std::string name : {"tone.ogg", "notes.ogg", "tone.mp3", "tone-without-header.mp3", "tone.flac"}) {
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

TEST_F(StreamedSoundTest, SeeksAnOggVorbisFileToTheExactFrame) {
    for (const std::string name : {"notes.ogg", "tone.ogg"}) {
        SCOPED_TRACE(name);
        const std::vector<std::uint8_t> file = readTestFile(name);
        const Sound decoded = Sound::decode(file);
        const std::vector<float>& expected = decoded.getData()->samples;
        const std::uint64_t length = decoded.getFrameCount();
        const std::uint32_t channels = decoded.getChannels();
        const auto reader = std::make_shared<LowestReadReader>(file);
        ReaderDecoder decoder(reader);

        // Seeks go back and forth, near the start, where the first packets only prime the decoder, near the end, and to scattered frames of the whole file.
        std::vector<std::uint64_t> frames{length / 2, length / 3, 0, 1, 1023, 1024, 2047, 2048, 4097, length - 1, length - 9, length - 2048, length / 2 + 1, 600, length / 2 - 1};
        for (std::uint64_t step = 1; step <= 60; ++step) {
            frames.push_back(step * 2654435761U % length);
        }
        std::vector<float> block(std::size_t{3000} * channels);
        for (const std::uint64_t frame : frames) {
            reader->reset();
            ASSERT_TRUE(decoder.seek(frame)) << frame;

            // A seek into the last quarter of the file starts at a page before the frame and never reads the start of the file again.
            if (frame >= length * 3 / 4) {
                EXPECT_GT(reader->getLowest(), 0U) << frame;
            }
            const std::uint64_t count = decoder.read(block);
            ASSERT_EQ(count, std::min<std::uint64_t>(3000, length - frame)) << frame;
            ASSERT_TRUE(std::equal(block.begin(), block.begin() + static_cast<std::ptrdiff_t>(count * channels), expected.begin() + static_cast<std::ptrdiff_t>(frame * channels))) << "The frames after frame " << frame << " differ from a decoding of the whole file.";
        }

        // The end of the file is a place to seek to, with no frames after it, and frames past it are not.
        EXPECT_TRUE(decoder.seek(length));
        EXPECT_EQ(decoder.read(block), 0U);
        EXPECT_FALSE(decoder.seek(length + 1));
        EXPECT_TRUE(decoder.seek(7));
        ASSERT_EQ(decoder.read(block), 3000U);
        EXPECT_TRUE(std::equal(block.begin(), block.end(), expected.begin() + static_cast<std::ptrdiff_t>(7 * channels)));
    }
}

TEST_F(StreamedSoundTest, LoopsAnOggVorbisFileBetweenLoopPointsExactly) {
    const std::vector<std::uint8_t> file = readTestFile("notes.ogg");
    const Sound decoded = Sound::decode(file);
    const std::vector<float>& expected = decoded.getData()->samples;
    const std::uint32_t channels = decoded.getChannels();
    const std::uint64_t loopStart = decoded.getFrameCount() / 5 + 17;
    const std::uint64_t loopEnd = decoded.getFrameCount() * 3 / 4 + 5;

    // The decoder plays to the end of the loop and goes on from its start, as the fill of a looping voice does, three times over.
    ReaderDecoder decoder(makeReader(file));
    std::vector<float> block(std::size_t{4096} * channels);
    std::uint64_t position = 0;
    for (int pass = 0; pass < 3; ++pass) {
        while (position < loopEnd) {
            const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(4096, loopEnd - position));
            ASSERT_EQ(decoder.read(std::span(block).first(count * channels)), count);
            ASSERT_TRUE(std::equal(block.begin(), block.begin() + static_cast<std::ptrdiff_t>(count * channels), expected.begin() + static_cast<std::ptrdiff_t>(position * channels))) << pass << " " << position;
            position += count;
        }
        ASSERT_TRUE(decoder.seek(loopStart));
        position = loopStart;
    }
}

TEST_F(StreamedSoundTest, SeeksAnOggVorbisFileByReadingOnlyThePagesAroundTheFrame) {
    const std::vector<std::uint8_t> file = readTestFile("notes.ogg");
    std::map<std::string, std::vector<std::uint8_t>> files;
    files.emplace("content/music/notes.ogg", file);
    const auto package = std::make_shared<test::CountingPackage>(std::make_shared<io::MemoryPackage>("music", std::move(files)));
    const Sound decoded = Sound::decode(file);
    const std::uint64_t length = decoded.getFrameCount();
    std::vector<float> block(std::size_t{256} * decoded.getChannels());

    // A decoder that never read the end of the file finds the page before the frame by bisecting the file.
    ReaderDecoder fresh(package->openAssetReader("music/notes.ogg"));
    package->reset();
    ASSERT_TRUE(fresh.seek(length - 5000));
    ASSERT_EQ(fresh.read(block), 256U);
    EXPECT_LT(package->getReadBytes(), file.size() / 2);

    // A decoder that played the file seeks back near its end through the pages it recorded.
    ReaderDecoder played(package->openAssetReader("music/notes.ogg"));
    while (played.read(block) > 0) {}
    package->reset();
    ASSERT_TRUE(played.seek(length - 5000));
    ASSERT_EQ(played.read(block), 256U);
    EXPECT_LT(package->getReadBytes(), file.size() / 8);
    EXPECT_LE(package->getLargestRead(), ReaderDecoder::kWindowSize);
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
