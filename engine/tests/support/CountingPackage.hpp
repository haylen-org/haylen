#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "haylen/io/Package.hpp"

namespace haylen::test {

// Passes a package through and counts what its readers read and the readers that are open, so tests can show which bytes of a file an operation touches and how many files it holds open. It also counts apart the reads of one thread it watches, such as the thread that mixes audio, so tests can show that the thread never reads.
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
    [[nodiscard]] std::size_t getOpenReaders() const noexcept {
        return counters->open;
    }
    [[nodiscard]] std::size_t getMostOpenReaders() const noexcept {
        return counters->mostOpen;
    }
    [[nodiscard]] std::uint64_t getWatchedReads() const noexcept {
        return counters->watchedReads;
    }
    void watch(std::thread::id thread) noexcept;
    void reset() noexcept;

  private:
    struct Counters {
        std::atomic<std::uint64_t> bytes{0};
        std::atomic<std::uint64_t> largest{0};
        std::atomic<std::size_t> open{0};
        std::atomic<std::size_t> mostOpen{0};
        std::atomic<std::thread::id> watched;
        std::atomic<std::uint64_t> watchedReads{0};
    };

    class Reader final : public io::PackageReader {
      public:
        Reader(std::unique_ptr<io::PackageReader> innerReader, std::shared_ptr<Counters> sharedCounters);
        ~Reader() override;

        Reader(const Reader&) = delete;
        Reader& operator=(const Reader&) = delete;

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
