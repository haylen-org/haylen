#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::storage {

// Private, writable files of the current user and app. Writes are atomic, and flush makes them durable on platforms that buffer storage, such as the browser.
class UserStorage final {
  public:
    explicit UserStorage(std::filesystem::path folder, std::function<void()> onFlush = {});

    [[nodiscard]] const std::filesystem::path& getRoot() const noexcept {
        return root;
    }
    [[nodiscard]] bool exists(std::string_view path) const;
    [[nodiscard]] std::vector<std::uint8_t> read(std::string_view path) const;
    [[nodiscard]] std::string readText(std::string_view path) const;
    void write(std::string_view path, std::span<const std::uint8_t> bytes);
    void writeText(std::string_view path, std::string_view text);
    bool remove(std::string_view path);
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const;
    void flush();

  private:
    [[nodiscard]] std::filesystem::path resolve(std::string_view path) const;

    std::filesystem::path root;
    std::function<void()> persist;
};

} // namespace haylen::storage
