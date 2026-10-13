#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::storage {

// Private, writable files of the current user and app. Writes are atomic, and `flush` makes them durable on platforms that buffer storage, such as the browser. A durable write also flushes the file and its folder to the disk before and after it replaces the file, so a loss of power leaves the old file or the new one, never an empty one. Temporary files that a crash left behind go when the storage opens. Reads, writes, lists and removes may run on any thread, and two writes of the same file never mix, while `flush` runs on the frame thread.
class UserStorage final {
  public:
    enum class Durability : std::uint8_t {
        Atomic,
        Durable,
    };

    explicit UserStorage(std::filesystem::path folder, std::function<void()> onFlush = {});

    [[nodiscard]] const std::filesystem::path& getRoot() const noexcept {
        return root;
    }
    [[nodiscard]] bool exists(std::string_view path) const;
    [[nodiscard]] std::vector<std::uint8_t> read(std::string_view path) const;
    [[nodiscard]] std::string readText(std::string_view path) const;
    void write(std::string_view path, std::span<const std::uint8_t> bytes, Durability durability = Durability::Atomic);
    void writeText(std::string_view path, std::string_view text, Durability durability = Durability::Atomic);
    bool remove(std::string_view path);
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const;
    void flush();

  private:
    static constexpr int kReplaceAttempts = 20;
    static constexpr std::chrono::milliseconds kReplaceDelay{25};

    [[nodiscard]] std::filesystem::path resolve(std::string_view path) const;

    // Writes the bytes to the temporary sibling and moves it over the file, and returns whether every step succeeded.
    [[nodiscard]] bool replace(const std::filesystem::path& file, const std::filesystem::path& temporary, std::span<const std::uint8_t> bytes, Durability durability);
    [[nodiscard]] static bool writeDurably(const std::filesystem::path& file, std::span<const std::uint8_t> bytes);
    void removeTemporaryFiles() noexcept;

    std::filesystem::path root;
    std::function<void()> persist;
    std::atomic<std::uint64_t> writeCount = 0;
    mutable std::shared_mutex mutex;
};

} // namespace haylen::storage
