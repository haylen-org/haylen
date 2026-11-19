#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/input/InputPlayback.hpp"
#include "haylen/input/InputRecording.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::input {

class InputRecordingTest : public ::testing::Test {
  protected:
    [[nodiscard]] static InputRecording makeRecording() {
        return InputRecording({.buttons = {"left", "right", "jump"}, .players = 2, .identifier = "com.haylen.tests", .fixedRate = 30.0, .data = "seed"});
    }

    [[nodiscard]] static std::string malformed(const std::vector<std::byte>& bytes) {
        try {
            (void)InputRecording::fromBytes(bytes);
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return {};
    }
};

TEST_F(InputRecordingTest, KeepsEveryStepThroughItsBytes) {
    InputRecording recording = makeRecording();
    for (std::uint32_t step = 0; step < 10000; ++step) {
        recording.write(0, step < 5000 ? 0b010U : 0b101U);
        recording.write(1, step % 7 == 0 ? 0b100U : 0U);
        recording.advance(step * 3U);
    }

    const std::vector<std::byte> bytes = recording.toBytes();
    const InputRecording loaded = InputRecording::fromBytes(bytes);
    EXPECT_EQ(loaded.getStepCount(), 10000U);
    EXPECT_EQ(loaded.getHeader().buttons, recording.getHeader().buttons);
    EXPECT_EQ(loaded.getHeader().players, 2U);
    EXPECT_EQ(loaded.getHeader().identifier, "com.haylen.tests");
    EXPECT_EQ(loaded.getHeader().fixedRate, 30.0);
    EXPECT_EQ(loaded.getHeader().data, "seed");

    for (std::uint32_t step = 0; step < 10000; step += 13) {
        EXPECT_EQ(loaded.getMask(step, 0), recording.getMask(step, 0));
        EXPECT_EQ(loaded.getMask(step, 1), recording.getMask(step, 1));
        EXPECT_EQ(loaded.getHash(step), step * 3U);
    }

    EXPECT_EQ(loaded.toBytes(), bytes);

    // A button held for a long time costs one run, however many steps it lasts.
    InputRecording held({.buttons = {"jump"}});
    for (int step = 0; step < 100000; ++step) {
        held.write(0, 1);
        held.advance();
    }

    EXPECT_LT(held.toBytes().size(), 64U);
    EXPECT_FALSE(held.getHash(0).has_value());
    EXPECT_THROW(held.write(1, 0), std::out_of_range);
    EXPECT_THROW((void)held.getMask(100000, 0), std::out_of_range);
}

TEST_F(InputRecordingTest, PlaysTheStepsBackAndSeeks) {
    auto recording = std::make_shared<InputRecording>(makeRecording());
    for (std::uint32_t step = 0; step < 4; ++step) {
        recording->write(0, step);
        recording->advance();
    }

    InputPlayback playback(recording);
    std::vector<std::uint32_t> played;
    while (!playback.isFinished()) {
        ASSERT_LT(played.size(), 4U) << "The playback goes on past the steps of its recording.";
        played.push_back(playback.getMask(0));
        playback.advance();
    }

    EXPECT_EQ(played, (std::vector<std::uint32_t>{0, 1, 2, 3}));
    EXPECT_EQ(playback.getMask(0), 0U);
    playback.seek(2);
    EXPECT_EQ(playback.getMask(0), 2U);
    EXPECT_THROW(playback.seek(5), std::out_of_range);
    EXPECT_THROW((void)playback.getMask(2), std::out_of_range);
}

TEST_F(InputRecordingTest, RefusesMalformedBytes) {
    InputRecording recording = makeRecording();
    recording.write(0, 3);
    recording.advance();
    std::vector<std::byte> bytes = recording.toBytes();

    EXPECT_EQ(malformed({}), "The input recording is malformed: it ends before its last number.");
    EXPECT_EQ(malformed({std::byte{1}}), "The input recording is malformed: it does not start like one.");
    std::vector<std::byte> truncated(bytes.begin(), bytes.end() - 1);
    EXPECT_EQ(malformed(truncated), "The input recording is malformed: it ends before its last number.");
    std::vector<std::byte> trailing = bytes;
    trailing.push_back(std::byte{0});
    EXPECT_EQ(malformed(trailing), "The input recording is malformed: bytes follow its last step.");
    std::vector<std::byte> endless(bytes.begin(), bytes.begin() + 5);
    endless.insert(endless.end(), 11, std::byte{0xFF});
    EXPECT_NE(malformed(endless).find("The input recording is malformed: "), std::string::npos);

    // A tiny file that claims the most steps in one run decodes without holding a mask per step.
    InputRecording single({.buttons = {"jump"}});
    single.write(0, 1);
    single.advance();
    std::vector<std::byte> claimed = single.toBytes();
    const std::size_t header = claimed.size() - 4;
    claimed.resize(header);
    for (const std::uint64_t value : {std::uint64_t{InputRecording::kMaxSteps}, std::uint64_t{0}, std::uint64_t{InputRecording::kMaxSteps}, std::uint64_t{1}}) {
        std::uint64_t rest = value;
        do {
            const auto low = static_cast<std::uint8_t>(rest & 0x7FU);
            rest >>= 7U;
            claimed.push_back(static_cast<std::byte>(rest != 0 ? low | 0x80U : low));
        } while (rest != 0);
    }

    const InputRecording huge = InputRecording::fromBytes(claimed);
    EXPECT_EQ(huge.getStepCount(), InputRecording::kMaxSteps);
    EXPECT_EQ(huge.getMask(InputRecording::kMaxSteps - 1, 0), 1U);

    // A step that presses a button past the buttons of the recording would reach a command buffer that does not have it.
    std::vector<std::byte> unnamed = single.toBytes();
    unnamed.back() = std::byte{2};
    EXPECT_EQ(malformed(unnamed), "The input recording is malformed: a number is out of range.");
    EXPECT_THROW(single.write(0, 2), std::invalid_argument);

    EXPECT_THROW(InputRecording({.buttons = {}}), std::invalid_argument);
    EXPECT_THROW(InputRecording({.buttons = {"a"}, .players = 17}), std::invalid_argument);
}

TEST(RandomStateTest, RepeatsTheSequenceFromARestoredState) {
    math::Random random(1234);
    (void)random.nextU64();
    const auto saved = random.getState();
    const std::uint64_t first = random.nextU64();
    const std::uint64_t second = random.nextU64();
    random.setState(saved);
    EXPECT_EQ(random.nextU64(), first);
    EXPECT_EQ(random.nextU64(), second);
    EXPECT_THROW(random.setState({}), std::invalid_argument);
}

} // namespace haylen::input
