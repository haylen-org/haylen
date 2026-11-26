#pragma once

#include <filesystem>
#include <string>

namespace haylen::test {

// Creates a unique folder under the system temporary directory and removes it afterwards. A folder that cannot go, such as one with a file that the test still holds open on Windows, fails the test with its path and the reason.
class TemporaryDirectory final {
  public:
    TemporaryDirectory();
    ~TemporaryDirectory();

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& getPath() const noexcept {
        return path;
    }
    void write(const std::string& relative, const std::string& content) const;

  private:
    [[nodiscard]] static std::filesystem::path makeUniquePath();

    std::filesystem::path path;
};

} // namespace haylen::test
