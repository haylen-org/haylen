#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>
#include <string_view>

#include "platform/SystemFontReader.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::platform {

// The reader hands over the whole file of a face, and a file the app cannot read is an error.
TEST(SystemFontReaderTest, ReadsTheWholeFileOrFails) {
    const test::TemporaryDirectory directory;
    const std::filesystem::path& folder = directory.getPath();
    const std::string_view bytes = "font";
    test::TestFiles::write(folder / "face.ttf", bytes);

    EXPECT_EQ(SystemFontReader::read(folder / "face.ttf").size(), bytes.size());
    EXPECT_THROW((void)SystemFontReader::read(folder / "missing.ttf"), std::runtime_error);
}

} // namespace haylen::platform
