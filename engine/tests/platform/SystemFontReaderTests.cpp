#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

#include "platform/SystemFontReader.hpp"
#include "support/TestFiles.hpp"

namespace haylen::platform {

// The reader hands over the whole file of a face, and a file the app cannot read is an error.
TEST(SystemFontReaderTest, ReadsTheWholeFileOrFails) {
    const std::filesystem::path folder = std::filesystem::temp_directory_path() / ("haylen-system-font-" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
    std::filesystem::create_directories(folder);
    const std::string_view bytes = "font";
    test::TestFiles::write(folder / "face.ttf", bytes);

    EXPECT_EQ(SystemFontReader::read(folder / "face.ttf").size(), bytes.size());
    EXPECT_THROW((void)SystemFontReader::read(folder / "missing.ttf"), std::runtime_error);
    std::filesystem::remove_all(folder);
}

} // namespace haylen::platform
