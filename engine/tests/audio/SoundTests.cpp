#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "audio/MixerFixture.hpp"
#include "audio/SoundData.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Json.hpp"
#include "support/TestFiles.hpp"

namespace haylen::audio {

class SoundTest : public MixerFixture {};

TEST_F(SoundTest, DecodesAndStreamsAudioFiles) {
    const std::vector<std::uint8_t> file = test::TestFiles::wav(22050, 2, makeSignal(2205, 2, 0.25F));

    const Sound decoded = Sound::decode(file);
    EXPECT_TRUE(decoded.isValid());
    EXPECT_FALSE(decoded.isStreamed());
    EXPECT_EQ(decoded.getChannels(), 2U);
    EXPECT_EQ(decoded.getSampleRate(), 22050U);
    EXPECT_EQ(decoded.getFrameCount(), 2205U);
    EXPECT_NEAR(decoded.getDuration(), 0.1F, 0.0001F);

    const Sound streamed = Sound::stream(makeReader(file));
    EXPECT_TRUE(streamed.isStreamed());
    EXPECT_EQ(streamed.getFrameCount(), 2205U);
    EXPECT_NE(streamed, decoded);

    const Sound empty;
    EXPECT_FALSE(empty.isValid());
    EXPECT_EQ(empty.getDuration(), 0.0F);
    EXPECT_EQ(empty.getChannels(), 0U);
    EXPECT_THROW((void)Sound::decode(test::TestFiles::bytes("not audio")), std::runtime_error);
    EXPECT_THROW((void)Sound::stream(makeReader({})), std::runtime_error);
    EXPECT_THROW((void)Sound::stream(nullptr), std::invalid_argument);
    EXPECT_THROW((void)Sound::decode(test::TestFiles::wav(kRate, 1, {})), std::runtime_error);
}

TEST_F(SoundTest, MakesSoundsFromRawSamples) {
    const Sound floats = Sound::fromSamples(std::vector<float>{0.5F, -0.5F, 0.25F, -0.25F}, 2, 24000);
    EXPECT_FALSE(floats.isStreamed());
    EXPECT_EQ(floats.getChannels(), 2U);
    EXPECT_EQ(floats.getSampleRate(), 24000U);
    EXPECT_EQ(floats.getFrameCount(), 2U);
    EXPECT_EQ(floats.getData()->samples, (std::vector<float>{0.5F, -0.5F, 0.25F, -0.25F}));

    const Sound shorts = Sound::fromSamples(std::vector<std::int16_t>{16384, -32768, 0}, 1, 8000);
    EXPECT_EQ(shorts.getData()->samples, (std::vector<float>{0.5F, -1.0F, 0.0F}));
    EXPECT_NEAR(shorts.getDuration(), 3.0F / 8000.0F, 1e-7F);

    EXPECT_THROW((void)Sound::fromSamples(std::vector<float>{0.5F, 0.5F, 0.5F}, 2, 8000), std::invalid_argument);
    EXPECT_THROW((void)Sound::fromSamples(std::vector<float>{0.5F}, 1, 0), std::invalid_argument);
    EXPECT_THROW((void)Sound::fromSamples(std::vector<float>{}, 1, 8000), std::runtime_error);
}

TEST_F(SoundTest, ReadsTheSessionFromHaylenJson) {
    const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(R"({"audio": {"iosSession": "playback", "mixWithOthers": true}})"));
    EXPECT_EQ(config.audioSession.category, Session::Category::Playback);
    EXPECT_TRUE(config.audioSession.mixWithOthers);
    EXPECT_EQ(config.toJson().at("audio"), core::Json::parse(R"({"iosSession": "playback", "mixWithOthers": true, "background": false, "latency": "balanced", "limiter": true})"));
    EXPECT_TRUE(core::AppConfig::fromJson(core::Json::parse(R"({"audio": {"iosSession": "playback", "background": true}})")).audioSession.background);
    const core::AppConfig output = core::AppConfig::fromJson(core::Json::parse(R"({"audio": {"latency": "powerSaving", "limiter": false}})"));
    EXPECT_EQ(output.audioOutput.latency, OutputSettings::Latency::PowerSaving);
    EXPECT_FALSE(output.audioOutput.limiter);
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"audio": {"latency": "fast"}})")), std::invalid_argument);
    EXPECT_EQ(core::AppConfig::fromJson(core::Json::parse(R"({"audio": {"iosSession": "soloAmbient"}})")).toJson().at("audio").at("iosSession"), "soloAmbient");
    EXPECT_EQ(core::AppConfig{}.audioSession.category, Session::Category::Ambient);

    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"audio": {"iosSession": "record"}})")), std::invalid_argument);
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"audio": {"iosSession": ""}})")), std::invalid_argument);
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"audio": {"focus": true}})")), std::invalid_argument);
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"audio": {"mixWithOthers": true}})")), std::invalid_argument);
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"audio": {"background": true}})")), std::invalid_argument);
    EXPECT_THROW(Mixer({.device = false, .session = {.category = Session::Category::Ambient, .mixWithOthers = true}}, jobs), std::invalid_argument);
    EXPECT_NO_THROW(Mixer({.device = false, .session = {.category = Session::Category::Playback, .mixWithOthers = true}}, jobs));
}

} // namespace haylen::audio
