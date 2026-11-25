#include <gtest/gtest.h>

#include <miniaudio.h>

#include <array>
#include <string_view>
#include <utility>

#include "audio/Device.hpp"

namespace haylen::audio {

// Tells why the system did not open the audio output, in sentences of Haylen that never repeat the text of the audio library.
class AudioDeviceTest : public ::testing::Test {
  protected:
    static constexpr std::array kResults = {
        std::pair{MA_NO_BACKEND, Device::Problem::NoDevice}, std::pair{MA_NO_DEVICE, Device::Problem::NoDevice}, std::pair{MA_DEVICE_TYPE_NOT_SUPPORTED, Device::Problem::NoDevice}, std::pair{MA_FAILED_TO_INIT_BACKEND, Device::Problem::NoDevice}, std::pair{MA_DOES_NOT_EXIST, Device::Problem::NoDevice}, std::pair{MA_ACCESS_DENIED, Device::Problem::Denied}, std::pair{MA_BUSY, Device::Problem::Busy}, std::pair{MA_ALREADY_IN_USE, Device::Problem::Busy}, std::pair{MA_SHARE_MODE_NOT_SUPPORTED, Device::Problem::Busy}, std::pair{MA_FORMAT_NOT_SUPPORTED, Device::Problem::UnsupportedFormat}, std::pair{MA_INVALID_DEVICE_CONFIG, Device::Problem::UnsupportedFormat}, std::pair{MA_OUT_OF_MEMORY, Device::Problem::OutOfMemory}, std::pair{MA_ERROR, Device::Problem::Failed}, std::pair{MA_FAILED_TO_OPEN_BACKEND_DEVICE, Device::Problem::Failed}, std::pair{MA_FAILED_TO_START_BACKEND_DEVICE, Device::Problem::Failed},
    };
};

TEST_F(AudioDeviceTest, TellsTheProblemOfEveryRefusalOfTheSystem) {
    for (const auto& [result, problem] : kResults) {
        EXPECT_EQ(Device::toProblem(result), problem) << ma_result_description(result);
    }
}

TEST_F(AudioDeviceTest, DescribesEveryProblemWithoutTheTextOfTheAudioLibrary) {
    for (const auto& [result, problem] : kResults) {
        const std::string_view sentence = Device::describe(problem);
        EXPECT_EQ(sentence.find(ma_result_description(result)), std::string_view::npos) << sentence;
    }
}

} // namespace haylen::audio
