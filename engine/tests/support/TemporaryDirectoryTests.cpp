#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "support/TemporaryDirectory.hpp"

namespace haylen::test {

// A temporary folder that cannot go fails the test that made it, so a file that a test leaves open never stays behind unnoticed.
class TemporaryDirectoryTest : public ::testing::Test {
  protected:
    // Holds a file of a folder open and takes every permission from the folder, which stops the removal of the folder on every system: Windows removes no file that a program holds open, and the other systems remove nothing from a folder without permissions.
    static void lock(const std::filesystem::path& folder, std::ofstream& held) {
        std::filesystem::create_directories(folder);
        held.open(folder / "held.txt");
        ASSERT_TRUE(held.is_open()) << folder;
        std::filesystem::permissions(folder, std::filesystem::perms::none);
    }

    static void unlock(const std::filesystem::path& folder, std::ofstream& held) {
        held.close();
        if (std::filesystem::exists(folder)) {
            std::filesystem::permissions(folder, std::filesystem::perms::owner_all);
        }
    }
};

TEST_F(TemporaryDirectoryTest, FailsTheTestWithThePathAndTheReasonWhenItCannotRemoveItsFolder) {
    // A user who removes every folder, such as the superuser, cannot meet the problem.
    const TemporaryDirectory probes;
    const std::filesystem::path probe = probes.getPath() / "locked";
    std::ofstream probeHeld;
    lock(probe, probeHeld);
    std::error_code removal;
    std::filesystem::remove_all(probe, removal);
    unlock(probe, probeHeld);
    if (!removal) {
        GTEST_SKIP() << "The user removes folders without permission, so no folder resists its removal.";
    }

    ::testing::TestPartResultArray failures;
    std::filesystem::path kept;
    std::ofstream held;
    {
        const ::testing::ScopedFakeTestPartResultReporter reporter(&failures);
        const TemporaryDirectory directory;
        kept = directory.getPath();
        lock(kept / "locked", held);
    }

    // The test lets go of the file it held on purpose, so the folder goes before the test ends.
    unlock(kept / "locked", held);
    std::filesystem::remove_all(kept);
    EXPECT_FALSE(std::filesystem::exists(kept)) << kept;
    ASSERT_EQ(failures.size(), 1);
    const std::string message = failures.GetTestPartResult(0).message();
    const std::string start = "Failed\nThe test could not remove its temporary folder \"" + kept.string() + "\": ";
    EXPECT_TRUE(message.starts_with(start)) << message;
    EXPECT_GT(message.size(), start.size()) << "The message gives the reason.";
}

} // namespace haylen::test
