#pragma once

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace haylen::lua::debug {

// Edits package files the way an editor does while a test watches them.
class PackageEditTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::string toText(const std::vector<std::uint8_t>& bytes) {
        return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    }

    // File times can be coarser than a test runs, so changes move the time forward explicitly.
    static void touch(const std::filesystem::path& file, int seconds) {
        std::filesystem::last_write_time(file, std::filesystem::last_write_time(file) + std::chrono::seconds(seconds));
    }
};

} // namespace haylen::lua::debug
