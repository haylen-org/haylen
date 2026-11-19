#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "support/TestFiles.hpp"

namespace haylen::test {

// The files that a release, a staged release or a publication tree lays out in a folder, read whole by their paths relative to it, so tests compare what the tools wrote.
class ReleaseFiles final {
  public:
    [[nodiscard]] static std::map<std::string, std::vector<std::uint8_t>> read(const std::filesystem::path& folder) {
        std::map<std::string, std::vector<std::uint8_t>> files;
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(folder)) {
            if (entry.is_regular_file()) {
                files.emplace(entry.path().lexically_relative(folder).generic_string(), TestFiles::read(entry.path()));
            }
        }

        return files;
    }
};

} // namespace haylen::test
