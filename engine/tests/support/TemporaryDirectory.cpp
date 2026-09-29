#include "support/TemporaryDirectory.hpp"

#include <atomic>
#include <fstream>
#include <random>

namespace haylen::test {

TemporaryDirectory::TemporaryDirectory() : path(makeUniquePath()) {
    std::filesystem::create_directories(path);
}

TemporaryDirectory::~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
}

void TemporaryDirectory::write(const std::string& relative, const std::string& content) const {
    const std::filesystem::path file = path / relative;
    std::filesystem::create_directories(file.parent_path());
    std::ofstream stream(file, std::ios::binary);
    stream << content;
}

std::filesystem::path TemporaryDirectory::makeUniquePath() {
    static std::atomic<int> counter{0};
    std::random_device device;
    return std::filesystem::temp_directory_path() / ("haylen-test-" + std::to_string(device()) + "-" + std::to_string(counter++));
}

} // namespace haylen::test
