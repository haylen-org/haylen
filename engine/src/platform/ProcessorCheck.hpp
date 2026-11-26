#pragma once

#include <cstdint>

namespace haylen::platform {

// Every x64 build of the engine compiles with the SSE4.2 and POPCNT instructions, so the entry points of Windows and Linux check them before anything else of the app runs. The answer of a processor is the ECX register of the leaf 1 of CPUID, which every x64 processor gives.
class ProcessorCheck final {
  public:
    static constexpr std::uint32_t kSse42 = 1U << 20;
    static constexpr std::uint32_t kPopcnt = 1U << 23;

    // Returns the sentence that names the instructions that the features lack, or null when they hold both.
    [[nodiscard]] static const char* describeMissing(std::uint32_t features) noexcept;

    // Returns the features of this processor, which hold both instructions on a processor that is not x64.
    [[nodiscard]] static std::uint32_t readFeatures() noexcept;

    // Writes the sentence of the missing instructions to the standard error and returns `false` when this processor lacks any of them.
    [[nodiscard]] static bool require() noexcept;

  private:
    static constexpr const char* kMissingBoth = "This processor lacks the SSE4.2 and POPCNT instructions, which Haylen apps need on x64 processors. Every x64 processor made since 2011 has them.";
    static constexpr const char* kMissingSse42 = "This processor lacks the SSE4.2 instructions, which Haylen apps need on x64 processors. Every x64 processor made since 2011 has them.";
    static constexpr const char* kMissingPopcnt = "This processor lacks the POPCNT instruction, which Haylen apps need on x64 processors. Every x64 processor made since 2011 has it.";
};

} // namespace haylen::platform
