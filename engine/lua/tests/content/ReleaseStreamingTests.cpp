#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "audio/MixerFixture.hpp"
#include "content/BytecodeCompiler.hpp"
#include "content/format/Chunker.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/audio/Sound.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/lua/Application.hpp"
#include "support/CountingPackage.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::content {

// Plays sounds that stream from a protected release, whose reader decrypts the chunks that a range covers and nothing else. The release is the first base, so its folder goes only after the mixer of the second base let go of the voices whose readers hold its files open.
class ReleaseStreamingTest : protected test::ReleaseFixture, public haylen::audio::MixerFixture {
  protected:
    ReleaseStreamingTest() : test::ReleaseFixture(std::make_shared<lua::content::BytecodeCompiler>(), lua::Application::kSections) {}
};

TEST_F(ReleaseStreamingTest, DecryptsOnlyTheChunksAroundTheCursorOfAVoice) {
    constexpr std::uint64_t kFrames = std::uint64_t{1} << 21;
    std::map<std::string, std::vector<std::uint8_t>> files;
    files.emplace("haylen.json", test::TestFiles::bytes(R"({"name": "Release Test", "identifier": "com.haylen.tests"})"));
    files.emplace("source/main.lua", test::TestFiles::bytes("return {}"));
    files.emplace("content/music/long.wav", makePositions(kFrames));
    build(haylen::io::MemoryPackage("source", std::move(files)));
    const auto counting = std::make_shared<test::CountingPackage>(getFiles());
    const std::shared_ptr<haylen::io::Package> package = open(counting);
    std::uint64_t shardBytes = 0;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(getFolder())) {
        shardBytes += entry.path().extension() == ".hpak" ? entry.file_size() : 0;
    }

    // The sound reads the header of its file, which decrypts the first chunk alone.
    counting->reset();
    const haylen::audio::Sound sound = haylen::audio::Sound::stream(package->openAssetReader("music/long.wav"));
    EXPECT_EQ(sound.getFrameCount(), kFrames);
    EXPECT_LT(counting->getReadBytes(), haylen::content::Chunker::kMaximumSize);

    // A voice that starts half a minute in decrypts the chunks of that part of the file, and the thread that mixes never reads.
    counting->watch(std::this_thread::get_id());
    counting->reset();
    const haylen::audio::Mixer::VoiceId voice = mixer.play(sound, {.startAt = 30.0F});
    const std::vector<std::uint64_t> played = listen(voice, kRate / 2 + 4096);
    const auto first = std::ranges::find(played, 30 * kRate);
    ASSERT_NE(first, played.end());
    EXPECT_GE(std::distance(first, played.end()), std::ptrdiff_t{kRate / 2});
    EXPECT_GT(counting->getReadBytes(), 0U);
    EXPECT_LT(counting->getReadBytes(), shardBytes / 2);
    EXPECT_EQ(counting->getWatchedReads(), 0U);
}

} // namespace haylen::lua::content
