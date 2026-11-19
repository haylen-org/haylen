#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace haylen::test {

// Writes the zip archives that tests open as packages, with libzip, and throws when libzip refuses an entry or the archive, so a test never opens an archive it did not mean.
class ZipArchives final {
  public:
    struct Entry {
        std::string name;
        std::vector<std::uint8_t> bytes;

        // Compresses the entry with deflate whatever libzip would choose, for a test that reads the compressed bytes.
        bool deflate = false;
    };

    static void write(const std::filesystem::path& file, const std::vector<Entry>& entries);
};

} // namespace haylen::test
