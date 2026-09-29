#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <vector>

#include "audio/Freeverb.hpp"
#include "audio/MixerFixture.hpp"
#include "haylen/audio/Delay.hpp"
#include "haylen/audio/Filter.hpp"
#include "haylen/audio/Reverb.hpp"

namespace haylen::audio {

class AudioEffectTest : public MixerFixture {
  protected:
    // Plays a sine through the effect and returns its amplitude once the effect settled, from the power of the left channel.
    float measure(const std::shared_ptr<Effect>& effect, float frequency, float amplitude = 0.5F) {
        const Mixer::VoiceId voice = mixer.play(makeSine(frequency, amplitude), {.loop = true, .effects = {effect}});
        peak(toFrames(0.1F));
        const float measured = amplitudeOf(renderLeft(toFrames(0.1F)));
        mixer.stop(voice);
        mixer.update(0.0F);
        return measured;
    }

    [[nodiscard]] static float amplitudeOf(const std::vector<float>& samples) {
        const double power = std::inner_product(samples.begin(), samples.end(), samples.begin(), 0.0) / static_cast<double>(samples.size());
        return static_cast<float>(std::sqrt(2.0 * power));
    }

    // Returns the index of the first sample louder than the threshold, or the size when none is.
    [[nodiscard]] static std::size_t firstLoud(const std::vector<float>& samples, float threshold, std::size_t from = 0) {
        const auto found = std::find_if(samples.begin() + static_cast<std::ptrdiff_t>(from), samples.end(), [threshold](float sample) { return std::abs(sample) > threshold; });
        return static_cast<std::size_t>(found - samples.begin());
    }
};

class FreeverbTest : public ::testing::Test {
  protected:
    static constexpr std::uint32_t kRate = 48000;

    // Runs an impulse on both channels through the reverb and returns the interleaved response.
    [[nodiscard]] static std::vector<float> respond(Freeverb& reverb, float seconds, std::uint32_t channels = 2) {
        const auto frames = static_cast<std::uint32_t>(seconds * static_cast<float>(kRate));
        std::vector<float> input(static_cast<std::size_t>(frames) * channels, 0.0F);
        std::fill_n(input.begin(), channels, 1.0F);
        std::vector<float> output(input.size());
        reverb.process(input.data(), output.data(), frames, channels);
        return output;
    }

    [[nodiscard]] static double energy(const std::vector<float>& samples, float from, float to) {
        const auto first = static_cast<std::size_t>(from * static_cast<float>(kRate)) * 2;
        const auto last = static_cast<std::size_t>(to * static_cast<float>(kRate)) * 2;
        double sum = 0.0;
        for (std::size_t index = first; index < last; ++index) {
            sum += static_cast<double>(samples[index]) * samples[index];
        }
        return sum;
    }
};

TEST_F(AudioEffectTest, FiltersShapeTheSpectrum) {
    const auto lowpass = std::make_shared<Filter>(Filter::Kind::Lowpass, Filter::Settings{.cutoff = 500.0F});
    EXPECT_NEAR(measure(lowpass, 100.0F), 0.5F, 0.03F);
    EXPECT_LT(measure(lowpass, 5000.0F), 0.02F);

    const auto highpass = std::make_shared<Filter>(Filter::Kind::Highpass, Filter::Settings{.cutoff = 500.0F});
    EXPECT_LT(measure(highpass, 100.0F), 0.03F);
    EXPECT_NEAR(measure(highpass, 5000.0F), 0.5F, 0.03F);

    const auto bandpass = std::make_shared<Filter>(Filter::Kind::Bandpass, Filter::Settings{.cutoff = 1000.0F, .q = 2.0F});
    EXPECT_NEAR(measure(bandpass, 1000.0F), 0.5F, 0.03F);
    EXPECT_LT(measure(bandpass, 100.0F), 0.05F);

    const auto notch = std::make_shared<Filter>(Filter::Kind::Notch, Filter::Settings{.cutoff = 1000.0F, .q = 1.0F});
    EXPECT_LT(measure(notch, 1000.0F), 0.01F);
    EXPECT_NEAR(measure(notch, 100.0F), 0.5F, 0.03F);

    const auto peaking = std::make_shared<Filter>(Filter::Kind::Peak, Filter::Settings{.cutoff = 1000.0F, .q = 1.0F, .gain = 12.0F});
    EXPECT_NEAR(measure(peaking, 1000.0F, 0.1F), 0.398F, 0.02F);
    EXPECT_NEAR(measure(peaking, 100.0F, 0.1F), 0.1F, 0.01F);

    const auto lowShelf = std::make_shared<Filter>(Filter::Kind::LowShelf, Filter::Settings{.cutoff = 1000.0F, .gain = -12.0F});
    EXPECT_NEAR(measure(lowShelf, 100.0F), 0.126F, 0.02F);
    EXPECT_NEAR(measure(lowShelf, 9600.0F), 0.5F, 0.03F);

    const auto highShelf = std::make_shared<Filter>(Filter::Kind::HighShelf, Filter::Settings{.cutoff = 1000.0F, .gain = 6.0F});
    EXPECT_NEAR(measure(highShelf, 9600.0F, 0.2F), 0.399F, 0.02F);
    EXPECT_NEAR(measure(highShelf, 100.0F, 0.2F), 0.2F, 0.01F);
}

TEST_F(AudioEffectTest, FilterParametersChangeWhileItPlays) {
    const auto lowpass = std::make_shared<Filter>(Filter::Kind::Lowpass, Filter::Settings{.cutoff = 200.0F});
    mixer.play(makeSine(5000.0F), {.loop = true, .effects = {lowpass}});
    peak(toFrames(0.05F));
    EXPECT_LT(amplitudeOf(renderLeft(toFrames(0.05F))), 0.01F);

    lowpass->setCutoff(20000.0F);
    lowpass->setQ(0.5F);
    peak(toFrames(0.05F));
    EXPECT_GT(amplitudeOf(renderLeft(toFrames(0.05F))), 0.4F);
    EXPECT_EQ(lowpass->getCutoff(), 20000.0F);
    EXPECT_EQ(lowpass->getQ(), 0.5F);
    EXPECT_EQ(lowpass->getKind(), Filter::Kind::Lowpass);
    EXPECT_EQ(lowpass->getTail(), 0.0F);

    EXPECT_FALSE(lowpass->hasGain());
    EXPECT_THROW(lowpass->setGain(3.0F), std::invalid_argument);
    EXPECT_THROW(lowpass->setCutoff(0.0F), std::invalid_argument);
    EXPECT_THROW(lowpass->setQ(-1.0F), std::invalid_argument);
    EXPECT_THROW(Filter(Filter::Kind::Notch, {.gain = 3.0F}), std::invalid_argument);
    EXPECT_THROW(Filter(Filter::Kind::Peak, {.cutoff = std::nanf("")}), std::invalid_argument);

    Filter shelf(Filter::Kind::HighShelf, {});
    shelf.setGain(-3.0F);
    EXPECT_EQ(shelf.getGain(), -3.0F);
    EXPECT_THROW(shelf.setGain(std::nanf("")), std::invalid_argument);
    EXPECT_THROW(Filter(Filter::Kind::Peak, {.gain = std::numeric_limits<float>::infinity()}), std::invalid_argument);
    EXPECT_EQ(shelf.getGain(), -3.0F);
}

TEST_F(AudioEffectTest, EffectsChainInOrderOnBusesAndVoices) {
    const auto cut = std::make_shared<Filter>(Filter::Kind::Peak, Filter::Settings{.cutoff = 1000.0F, .q = 1.0F, .gain = -20.0F});
    const auto boost = std::make_shared<Filter>(Filter::Kind::Peak, Filter::Settings{.cutoff = 1000.0F, .q = 1.0F, .gain = 6.0206F});
    mixer.addBusEffect("sfx", cut);
    mixer.addBusEffect("sfx", boost);
    EXPECT_EQ(mixer.getBusEffects("sfx"), (std::vector<std::shared_ptr<Effect>>{cut, boost}));
    EXPECT_TRUE(cut->isAttached());

    const Mixer::VoiceId voice = mixer.play(makeSine(1000.0F, 0.2F), {.loop = true});
    peak(toFrames(0.05F));
    EXPECT_NEAR(amplitudeOf(renderLeft(toFrames(0.05F))), 0.04F, 0.005F);

    mixer.removeBusEffect("sfx", *cut);
    mixer.removeBusEffect("sfx", *cut);
    EXPECT_FALSE(cut->isAttached());
    peak(toFrames(0.05F));
    EXPECT_NEAR(amplitudeOf(renderLeft(toFrames(0.05F))), 0.4F, 0.02F);

    // The same effect moves to the voice, before the bus.
    mixer.addEffect(voice, cut);
    EXPECT_EQ(mixer.getEffects(voice), (std::vector<std::shared_ptr<Effect>>{cut}));
    peak(toFrames(0.05F));
    EXPECT_NEAR(amplitudeOf(renderLeft(toFrames(0.05F))), 0.04F, 0.005F);
    mixer.removeEffect(voice, *cut);
    mixer.removeBusEffect("sfx", *boost);
    EXPECT_TRUE(mixer.getEffects(voice).empty());
    peak(toFrames(0.05F));
    EXPECT_NEAR(amplitudeOf(renderLeft(toFrames(0.05F))), 0.2F, 0.01F);

    mixer.addBusEffect("master", boost);
    EXPECT_THROW(mixer.addBusEffect("music", boost), std::invalid_argument);
    EXPECT_THROW(mixer.addEffect(voice, boost), std::invalid_argument);
    EXPECT_THROW(mixer.addBusEffect("sfx", nullptr), std::invalid_argument);
    EXPECT_THROW(mixer.addBusEffect("missing", cut), std::invalid_argument);
    mixer.addEffect(9999, cut);
    mixer.removeEffect(9999, *cut);
    EXPECT_TRUE(mixer.getEffects(9999).empty());

    Mixer other({.device = false, .sampleRate = kRate});
    EXPECT_THROW(other.addBusEffect("sfx", cut), std::invalid_argument);
}

TEST_F(AudioEffectTest, DelayRepeatsTheInputAfterItsTime) {
    const auto echo = std::make_shared<Delay>(Delay::Settings{.time = 0.01F, .feedback = 0.5F, .wet = 1.0F, .dry = 0.0F});
    mixer.addBusEffect("sfx", echo);
    mixer.play(makeImpulse(256, 0.8F));
    const std::vector<float> left = renderLeft(toFrames(0.05F));

    const std::size_t first = firstLoud(left, 0.05F);
    ASSERT_LT(first, left.size());
    EXPECT_NEAR(std::abs(left[first]), 0.8F, 0.02F);
    EXPECT_NEAR(std::abs(left[first + 480]), 0.4F, 0.02F);
    EXPECT_NEAR(std::abs(left[first + 960]), 0.2F, 0.02F);
    EXPECT_EQ(firstLoud(left, 0.05F, first + 1), first + 480);

    echo->setTime(0.5F);
    echo->setFeedback(0.25F);
    echo->setWet(0.5F);
    echo->setDry(0.8F);
    EXPECT_EQ(echo->getTime(), 0.5F);
    EXPECT_EQ(echo->getFeedback(), 0.25F);
    EXPECT_EQ(echo->getWet(), 0.5F);
    EXPECT_EQ(echo->getDry(), 0.8F);
    EXPECT_EQ(echo->getMaxTime(), 1.0F);
    EXPECT_NEAR(echo->getTail(), 0.5F * (1.0F + std::log(0.001F) / std::log(0.25F)), 1e-4F);

    EXPECT_THROW(echo->setTime(1.5F), std::invalid_argument);
    EXPECT_THROW(echo->setFeedback(1.0F), std::invalid_argument);
    EXPECT_THROW(echo->setWet(-0.1F), std::invalid_argument);
    EXPECT_THROW(Delay({.maxTime = 0.0F}), std::invalid_argument);
    EXPECT_THROW(Delay({.time = 3.0F, .maxTime = 20.0F}), std::invalid_argument);
    EXPECT_EQ(Delay({.time = 0.2F, .feedback = 0.0F}).getTail(), 0.2F);
}

TEST_F(AudioEffectTest, MovedEffectsForgetTheirEarlierInput) {
    const auto echo = std::make_shared<Delay>(Delay::Settings{.time = 0.5F, .feedback = 0.0F, .wet = 1.0F, .dry = 0.0F});
    mixer.addBusEffect("sfx", echo);
    mixer.play(makeImpulse(64, 0.8F));
    EXPECT_EQ(peak(toFrames(0.05F)), 0.0F) << "the repeat is still ahead";

    // The impulse went into the delay on sfx, and on ambience, where nothing plays, its repeat never comes out.
    mixer.removeBusEffect("sfx", *echo);
    mixer.addBusEffect("ambience", echo);
    EXPECT_EQ(peak(toFrames(0.8F)), 0.0F);
}

TEST_F(AudioEffectTest, VoiceEffectsRingOutAfterTheVoiceEnds) {
    const auto echo = std::make_shared<Delay>(Delay::Settings{.time = 0.05F, .feedback = 0.0F, .wet = 1.0F, .dry = 1.0F});
    mixer.play(makeImpulse(64, 0.8F), {.effects = {echo}});
    peak(toFrames(0.01F));
    mixer.update(0.01F);
    EXPECT_EQ(mixer.getVoiceCount(), 0U);
    EXPECT_TRUE(echo->isAttached());

    EXPECT_NEAR(peak(toFrames(0.06F)), 0.8F, 0.02F);
    mixer.update(0.06F);
    EXPECT_FALSE(echo->isAttached());
}

TEST_F(AudioEffectTest, ReverbRingsOnABus) {
    const auto hall = std::make_shared<Reverb>(Reverb::Settings{.roomSize = 0.8F, .wet = 1.0F, .dry = 0.0F});
    mixer.addBusEffect("ambience", hall);
    mixer.play(makeImpulse(64, 0.8F), {.bus = "ambience"});
    const std::vector<float> left = renderLeft(toFrames(0.3F));
    EXPECT_GT(firstLoud(left, 1e-4F), 1200U);
    EXPECT_LT(firstLoud(left, 1e-4F), left.size());

    hall->setRoomSize(0.2F);
    hall->setDamping(0.9F);
    hall->setWidth(0.5F);
    hall->setWet(0.5F);
    hall->setDry(0.5F);
    EXPECT_EQ(hall->getRoomSize(), 0.2F);
    EXPECT_EQ(hall->getDamping(), 0.9F);
    EXPECT_EQ(hall->getWidth(), 0.5F);
    EXPECT_EQ(hall->getWet(), 0.5F);
    EXPECT_EQ(hall->getDry(), 0.5F);
    EXPECT_NEAR(hall->getTail(), Freeverb::getDecayTime(0.2F), 1e-6F);
    EXPECT_GT(peak(toFrames(0.05F)), 0.0F);
    EXPECT_THROW(hall->setRoomSize(1.5F), std::invalid_argument);
    EXPECT_THROW(Reverb({.wet = -1.0F}), std::invalid_argument);

    // A reverb that moves to another bus keeps its settings and starts without its old tail.
    hall->setDry(0.0F);
    mixer.removeBusEffect("ambience", *hall);
    mixer.addBusEffect("sfx", hall);
    peak(2048);
    mixer.play(makeImpulse(64, 0.8F));
    const std::vector<float> moved = renderLeft(toFrames(0.3F));
    EXPECT_GT(firstLoud(moved, 1e-4F), 1200U);
    EXPECT_LT(firstLoud(moved, 1e-4F), moved.size());
}

TEST_F(FreeverbTest, ImpulseResponseArrivesLateAndDecays) {
    Freeverb reverb(kRate);
    reverb.setRoomSize(0.5F);
    reverb.setDamping(0.5F);
    reverb.setMix(1.0F, 0.0F, 1.0F);
    const std::vector<float> response = respond(reverb, 4.0F);
    EXPECT_TRUE(std::ranges::all_of(response, [](float sample) { return std::isfinite(sample); }));

    // Nothing comes out before the shortest comb filter, which is 1116 samples at 44100 hertz, and the right side comes 23 samples later.
    std::size_t firstLeft = 0;
    while (response[firstLeft * 2] == 0.0F) {
        ++firstLeft;
    }
    std::size_t firstRight = 0;
    while (response[firstRight * 2 + 1] == 0.0F) {
        ++firstRight;
    }
    EXPECT_EQ(firstLeft, 1215U);
    EXPECT_EQ(firstRight, 1240U);

    const double early = energy(response, 0.0F, 0.2F);
    const double middle = energy(response, 1.0F, 1.2F);
    const double late = energy(response, 3.5F, 3.7F);
    EXPECT_GT(early, 0.0);
    EXPECT_LT(middle, early * 0.01);
    EXPECT_LT(late, early * 1e-9);

    double difference = 0.0;
    for (std::size_t frame = 0; frame < response.size() / 2; ++frame) {
        difference += std::abs(response[frame * 2] - response[frame * 2 + 1]);
    }
    EXPECT_GT(difference, 0.1);
}

TEST_F(FreeverbTest, RoomSizeSetsTheDecay) {
    Freeverb small(kRate);
    small.setRoomSize(0.3F);
    small.setMix(1.0F, 0.0F, 1.0F);
    Freeverb large(kRate);
    large.setRoomSize(0.9F);
    large.setMix(1.0F, 0.0F, 1.0F);
    EXPECT_GT(energy(respond(large, 1.2F), 1.0F, 1.2F), energy(respond(small, 1.2F), 1.0F, 1.2F) * 100.0);

    EXPECT_NEAR(Freeverb::getDecayTime(0.5F), 1.473F, 0.01F);
    EXPECT_GT(Freeverb::getDecayTime(1.0F), Freeverb::getDecayTime(0.5F));
}

TEST_F(FreeverbTest, DryLevelPassesEveryChannel) {
    Freeverb reverb(22050);
    reverb.setMix(0.0F, 0.5F, 1.0F);
    const std::vector<float> input{1.0F, -1.0F, 0.5F, 0.25F, 0.0F, 1.0F};
    std::vector<float> output(input.size());
    reverb.process(input.data(), output.data(), 2, 3);
    EXPECT_EQ(output, (std::vector<float>{0.5F, -0.5F, 0.25F, 0.125F, 0.0F, 0.5F}));

    std::vector<float> mono(4, 0.0F);
    reverb.process(input.data(), mono.data(), 4, 1);
    EXPECT_EQ(mono, (std::vector<float>{0.5F, -0.5F, 0.25F, 0.125F}));
}

} // namespace haylen::audio
