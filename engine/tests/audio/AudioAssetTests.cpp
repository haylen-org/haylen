#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "audio/MixerFixture.hpp"
#include "haylen/assets/Manager.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::audio {

class AudioAssetTest : public MixerFixture {};

TEST_F(AudioAssetTest, LoadsSoundsThroughTheAssetManager) {
    const std::vector<std::uint8_t> wav = test::TestFiles::wav(kRate, 1, makeSignal(480, 1, 0.5F));
    test::EngineFixture fixture({{"content/sfx/hit.wav", std::string(wav.begin(), wav.end())}});
    assets::Manager& assets = fixture.engine().getAssets();

    EXPECT_EQ(assets.getTypeForPath("sfx/hit.wav"), "sound");
    const auto decoded = assets.load("sound", "sfx/hit.wav");
    EXPECT_EQ(assets.load("sound", "sfx/hit.wav", {{"stream", false}}), decoded);
    EXPECT_NE(assets.load("sound", "sfx/hit.wav", {{"stream", true}}), decoded);
    EXPECT_THROW((void)assets.load("sound", "sfx/hit.wav", {{"loop", true}}), std::invalid_argument);
    EXPECT_EQ(fixture.engine().getAudio().getSampleRate(), 48000U);
}

} // namespace haylen::audio
