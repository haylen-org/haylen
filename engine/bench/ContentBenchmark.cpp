#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <vector>

#include "content/FolderStoreFiles.hpp"
#include "content/LocalStore.hpp"
#include "content/RangePlanner.hpp"
#include "content/ReleaseBuilder.hpp"
#include "content/ReleasePackage.hpp"
#include "content/crypto/ContentKey.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/crypto/SigningKey.hpp"
#include "content/format/ChunkRecord.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/PackageReader.hpp"

namespace haylen::bench {

// Times the protected content of a release on this machine: building it, opening it, reading files in order and at random, finding files in the catalog, planning the ranges of an update and adding the records of one to the local store, with the largest allocation of each, so a regression in throughput or a reader that starts to hold whole files shows in the numbers.
class ContentBenchmark final {
  public:
    // Keeps the largest single allocation of the whole process while a measurement runs, which the replaced global allocation functions report, so a reader that holds a whole file shows at once.
    class AllocationMeter final {
      public:
        static void reset() noexcept {
            largest.store(0, std::memory_order_relaxed);
        }

        static void record(std::size_t size) noexcept {
            std::size_t seen = largest.load(std::memory_order_relaxed);
            while (size > seen && !largest.compare_exchange_weak(seen, size, std::memory_order_relaxed)) {}
        }

        [[nodiscard]] static std::size_t getLargest() noexcept {
            return largest.load(std::memory_order_relaxed);
        }

      private:
        static inline std::atomic<std::size_t> largest{0};
    };

    static int run() {
        const std::filesystem::path root = std::filesystem::temp_directory_path() / "haylen-content-benchmark";
        std::filesystem::remove_all(root);
        writeSource(root / "source");

        auto keys = std::make_shared<content::KeyRing>();
        const content::Digest keyId = keys->add(makeKey(1));
        auto signing = std::make_shared<const content::SigningKey>(makeKey(101));
        const content::ReleaseBuilder builder(keys, keyId, signing);
        const content::Compatibility compatibility{.application = content::Manifest::identifyApplication(kIdentifier), .appBuild = 1, .profile = "desktop"};
        const content::VerifyingKey trusted = signing->getVerifyingKey();

        std::printf("%-52s %16s %14s\n", "Measurement", "Result", "Largest alloc");
        const double sourceMegabytes = static_cast<double>(kLargeFile + kTextFile + kSmallFiles * kSmallFile) / kMegabyte;
        report("Build a release", [&] { (void)builder.build(*io::Package::openDirectory(root / "source"), root / "release", std::nullopt, {.profile = "desktop", .appBuild = 1}); }, sourceMegabytes, "MiB/s");

        std::unique_ptr<content::ReleasePackage> release;
        report("Open the release", [&] { release = content::ReleasePackage::open(io::Package::openDirectory(root / "release"), keys, std::span(&trusted, 1), compatibility); }, 0.0, "ms");

        report("Read an incompressible file in order", [&] { readInOrder(*release, "content/media/large.bin"); }, static_cast<double>(kLargeFile) / kMegabyte, "MiB/s");
        report("Read a compressible file in order", [&] { readInOrder(*release, "content/data/text.txt"); }, static_cast<double>(kTextFile) / kMegabyte, "MiB/s");
        report("Read 4 KiB at 2000 random offsets", [&] { readAtRandom(*release, "content/media/large.bin"); }, 0.0, "ms");
        report("Find 2000 files in the catalog", [&] { findFiles(*release); }, 0.0, "ms");
        report("Plan the ranges of 100000 records", [] { planRanges(); }, 0.0, "ms");
        report("Add 256 records of 256 KiB to the local store", [&] { addRecords(root / "store", compatibility.application); }, 256.0 * kRecordSize / kMegabyte, "MiB/s");

        std::filesystem::remove_all(root);
        return 0;
    }

  private:
    static constexpr const char* kIdentifier = "dev.haylen.contentbenchmark";
    static constexpr double kMegabyte = 1024.0 * 1024.0;
    static constexpr std::uint64_t kLargeFile = std::uint64_t{64} << 20;
    static constexpr std::uint64_t kTextFile = std::uint64_t{16} << 20;
    static constexpr int kSmallFiles = 2000;
    static constexpr std::uint64_t kSmallFile = 2048;
    static constexpr std::size_t kRecordSize = 256 * 1024;

    static std::array<std::uint8_t, 32> makeKey(std::uint8_t first) noexcept {
        std::array<std::uint8_t, 32> key{};
        for (std::size_t index = 0; index < key.size(); ++index) {
            key[index] = static_cast<std::uint8_t>(first + index);
        }
        return key;
    }

    static void writeFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    static std::vector<std::uint8_t> randomBytes(std::size_t size, std::uint32_t seed) {
        std::mt19937 generator(seed);
        std::vector<std::uint8_t> bytes(size);
        std::ranges::generate(bytes, [&generator] { return static_cast<std::uint8_t>(generator()); });
        return bytes;
    }

    // An app with one large incompressible file, one large text file that compresses well and many small files.
    static void writeSource(const std::filesystem::path& folder) {
        const std::string app = R"({"name": "Content Benchmark", "identifier": "dev.haylen.contentbenchmark"})";
        writeFile(folder / "app.json", {app.begin(), app.end()});
        writeFile(folder / "source" / "main.lua", {});
        writeFile(folder / "content" / "media" / "large.bin", randomBytes(kLargeFile, 1));
        std::vector<std::uint8_t> text;
        const std::string line = "The tide comes in over the reef while the gulls circle the harbor.\n";
        while (text.size() < kTextFile) {
            text.insert(text.end(), line.begin(), line.end());
        }
        text.resize(kTextFile);
        writeFile(folder / "content" / "data" / "text.txt", text);
        for (int index = 0; index < kSmallFiles; ++index) {
            writeFile(folder / "content" / "items" / ("item_" + std::to_string(index) + ".bin"), randomBytes(kSmallFile, static_cast<std::uint32_t>(index + 2)));
        }
    }

    static void report(const char* name, const std::function<void()>& work, double megabytes, const char* unit) {
        AllocationMeter::reset();
        const auto start = std::chrono::steady_clock::now();
        work();
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        const double result = megabytes > 0.0 ? megabytes / seconds : seconds * 1000.0;
        std::printf("%-52s %10.1f %-5s %10.2f MiB\n", name, result, unit, static_cast<double>(AllocationMeter::getLargest()) / kMegabyte);
    }

    static void readInOrder(const io::Package& package, const std::string& path) {
        const std::unique_ptr<io::PackageReader> reader = package.openReader(path);
        std::vector<std::uint8_t> buffer(1024 * 1024);
        for (std::uint64_t offset = 0; offset < reader->getSize(); offset += buffer.size()) {
            (void)reader->read(offset, buffer);
        }
    }

    static void readAtRandom(const io::Package& package, const std::string& path) {
        const std::unique_ptr<io::PackageReader> reader = package.openReader(path);
        std::mt19937_64 generator(7);
        std::uniform_int_distribution<std::uint64_t> offsets(0, reader->getSize() - 4096);
        std::vector<std::uint8_t> buffer(4096);
        for (int index = 0; index < 2000; ++index) {
            (void)reader->read(offsets(generator), buffer);
        }
    }

    static void findFiles(const io::Package& package) {
        std::size_t found = 0;
        for (int index = 0; index < kSmallFiles; ++index) {
            found += package.exists("content/items/item_" + std::to_string(index) + ".bin") ? 1U : 0U;
        }
        if (found != static_cast<std::size_t>(kSmallFiles)) {
            std::fprintf(stderr, "The catalog lost files.\n");
        }
    }

    static void planRanges() {
        std::vector<content::RangePlanner::Span> records;
        std::uint64_t offset = 0;
        std::mt19937_64 generator(11);
        std::uniform_int_distribution<std::uint64_t> sizes(4096, 1024 * 1024);
        for (int index = 0; index < 100000; ++index) {
            const std::uint64_t size = sizes(generator);
            if (index % 3 != 0) {
                records.push_back({.offset = offset, .size = size});
            }
            offset += size;
        }
        (void)content::RangePlanner::plan(records);
    }

    static void addRecords(const std::filesystem::path& folder, const content::Digest& application) {
        const content::ContentKey key(makeKey(1));
        content::KeyRing ring;
        const content::Digest keyId = ring.add(makeKey(1));
        std::vector<std::vector<std::uint8_t>> records;
        for (std::uint32_t index = 0; index < 256; ++index) {
            const std::vector<std::uint8_t> plain = randomBytes(kRecordSize, 100 + index);
            std::vector<std::uint8_t> ciphertext;
            const content::ChunkRecord record = content::ChunkRecord::seal(key, plain, content::ChunkRecord::identifyContent(plain), ciphertext);
            const content::ChunkRecord::Header header = record.serialize();
            std::vector<std::uint8_t> bytes(header.begin(), header.end());
            bytes.insert(bytes.end(), ciphertext.begin(), ciphertext.end());
            records.push_back(std::move(bytes));
        }
        content::LocalStore store(std::make_unique<content::FolderStoreFiles>(folder), application, 1);
        for (const std::vector<std::uint8_t>& record : records) {
            store.addRecord(keyId, record);
        }
        store.commit(store.getState());
    }
};

} // namespace haylen::bench

int main() {
    return haylen::bench::ContentBenchmark::run();
}

// The standard names the replaceable allocation functions, so the benchmark replaces them here, and they only forward to the meter and to `malloc` and `free`. The array and nothrow forms call these by default.
void* operator new(std::size_t size) {
    haylen::bench::ContentBenchmark::AllocationMeter::record(size);
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}
