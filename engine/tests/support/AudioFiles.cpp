#include "support/AudioFiles.hpp"

#include <filesystem>

#include "support/TestFiles.hpp"

namespace haylen::test {

std::vector<std::uint8_t> AudioFiles::read(std::string_view name) {
    return TestFiles::read(std::filesystem::path(HAYLEN_TEST_AUDIO) / name);
}

} // namespace haylen::test
