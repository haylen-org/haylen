#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "haylen/io/Package.hpp"

namespace haylen::test {

// Passes a package through and counts what its readers read, so tests can show which bytes of a file an operation touches.
class CountingPackage final : public io::Package {
  public:
    explicit CountingPackage(std::shared_ptr<const io::Package> innerPackage);

    [[nodiscard]] std::string_view getName() const noexcept override {
        return inner->getName();
    }
    [[nodiscard]] bool exists(std::string_view path) const override {
        return inner->exists(path);
    }
    [[nodiscard]] std::uint64_t getFileSize(std::string_view path) const override {
        return inner->getFileSize(path);
    }
    [[nodiscard]] std::unique_ptr<io::PackageReader> openReader(std::string_view path) const override;
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const override {
        return inner->list(directory);
    }

    [[nodiscard]] std::uint64_t getReadBytes() const noexcept {
        return counters->bytes;
    }
    [[nodiscard]] std::uint64_t getLargestRead() const noexcept {
        return counters->largest;
    }
    void reset() noexcept;

  private:
    struct Counters {
        std::atomic<std::uint64_t> bytes{0};
        std::atomic<std::uint64_t> largest{0};
    };

    class Reader final : public io::PackageReader {
      public:
        Reader(std::unique_ptr<io::PackageReader> innerReader, std::shared_ptr<Counters> sharedCounters);

        [[nodiscard]] std::uint64_t getSize() const noexcept override {
            return inner->getSize();
        }
        [[nodiscard]] std::size_t read(std::uint64_t offset, std::span<std::uint8_t> target) override;

      private:
        std::unique_ptr<io::PackageReader> inner;
        std::shared_ptr<Counters> counters;
    };

    std::shared_ptr<const io::Package> inner;
    std::shared_ptr<Counters> counters = std::make_shared<Counters>();
};

} // namespace haylen::test
