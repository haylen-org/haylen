#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "platform/ProcessorCheck.hpp"

namespace haylen::platform {

class ProcessorCheckTest : public ::testing::Test {
  protected:
    // The ECX of the leaf 1 of CPUID on a processor of 2007 that runs SSE4.1 alone.
    static constexpr std::uint32_t kSse41Only = 1U << 19;

    static std::string describe(std::uint32_t features) {
        const char* missing = ProcessorCheck::describeMissing(features);
        return missing == nullptr ? std::string() : std::string(missing);
    }
};

TEST_F(ProcessorCheckTest, AcceptsAProcessorWithBothInstructions) {
    EXPECT_EQ(ProcessorCheck::describeMissing(kSse41Only | ProcessorCheck::kSse42 | ProcessorCheck::kPopcnt), nullptr);
}

TEST_F(ProcessorCheckTest, NamesTheInstructionsThatTheProcessorLacks) {
    EXPECT_EQ(describe(kSse41Only), "This processor lacks the SSE4.2 and POPCNT instructions, which Haylen apps need on x64 processors. Every x64 processor made since 2011 has them.");
    EXPECT_EQ(describe(kSse41Only | ProcessorCheck::kPopcnt), "This processor lacks the SSE4.2 instructions, which Haylen apps need on x64 processors. Every x64 processor made since 2011 has them.");
    EXPECT_EQ(describe(kSse41Only | ProcessorCheck::kSse42), "This processor lacks the POPCNT instruction, which Haylen apps need on x64 processors. Every x64 processor made since 2011 has it.");
}

TEST_F(ProcessorCheckTest, FindsBothInstructionsOnTheProcessorThatRunsTheTests) {
    EXPECT_EQ(ProcessorCheck::describeMissing(ProcessorCheck::readFeatures()), nullptr);
    EXPECT_TRUE(ProcessorCheck::require());
}

} // namespace haylen::platform
