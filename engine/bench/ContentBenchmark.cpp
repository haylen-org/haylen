#include <lua.hpp>

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
#include <thread>
#include <vector>

#include "content/FolderStoreFiles.hpp"
#include "content/LocalStore.hpp"
#include "content/RangePlanner.hpp"
#include "content/ReleaseBuilder.hpp"
#include "content/ReleasePackage.hpp"
#include "content/crypto/ContentKey.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/crypto/SigningKey.hpp"
#include "content/crypto/Aead.hpp"
#include "content/format/ChunkRecord.hpp"
#include "content/format/Compression.hpp"
#include "content/format/Compressor.hpp"
#include "content/format/RecordSealer.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/PackageReader.hpp"

namespace haylen::bench {

// Times the protected content of a release on this machine: building it and an update of it, opening it, reading files in order, on several threads and at random, compressing and encrypting chunks alone, finding files in the catalog of a release of tens of thousands of files, loading Lua modules, maps and images from it, planning the ranges of an update and adding the records of one to the local store, with the largest allocation of each, so a regression in throughput or a reader that starts to hold whole files shows in the numbers.
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
        report("Read the incompressible file on 4 threads at once", [&] { readOnThreads(*release, "content/media/large.bin"); }, static_cast<double>(kLargeFile) / kMegabyte, "MiB/s");
        report("Read 4 KiB at 2000 random offsets", [&] { readAtRandom(*release, "content/media/large.bin"); }, 0.0, "ms");
        report("Find 2000 files in the catalog", [&] { findFiles(*release); }, 0.0, "ms");

        // The codec and the cipher alone, on the chunks of the text file, show what a read costs beyond them.
        const std::vector<std::uint8_t> text = release->read("content/data/text.txt");
        std::vector<content::Compressor::Encoded> encoded;
        report("Compress the text in chunks with Zstandard", [&] { encoded = compressChunks(text); }, static_cast<double>(text.size()) / kMegabyte, "MiB/s");
        report("Decompress the text in chunks", [&] { decompressChunks(encoded, text.size()); }, static_cast<double>(text.size()) / kMegabyte, "MiB/s");
        report("Encrypt and decrypt the text with XChaCha20-Poly1305", [&] { encryptChunks(text); }, 2.0 * static_cast<double>(text.size()) / kMegabyte, "MiB/s");

        // Lua modules ship as bytecode, and maps and images load from the release the way an app loads them.
        const std::string module = makeModule();
        report("Load a Lua module of 2 MiB from its text", [&] { loadChunk({reinterpret_cast<const std::uint8_t*>(module.data()), module.size()}, "t"); }, 0.0, "ms");
        report("Load the same module as bytecode from the release", [&] { loadChunk(release->read("source/world.lua"), "b"); }, 0.0, "ms");
        report("Read and parse a map of JSON from the release", [&] { (void)core::Json::parse(release->readAssetText("maps/island.json")); }, 0.0, "ms");
        report("Read and decode a 1024 px PNG from the release", [&] { (void)graphics::Image::decode(release->read("content/images/scene.png")); }, 0.0, "ms");
        report("Read the same pixels raw from the release", [&] { (void)release->read("content/images/scene.rgba"); }, 0.0, "ms");
        printValue("Bytes the PNG takes in the release", static_cast<double>(storedSize(*release, "content/images/scene.png")) / kMegabyte, "MiB");
        printValue("Bytes the raw pixels take in the release", static_cast<double>(storedSize(*release, "content/images/scene.rgba")) / kMegabyte, "MiB");

        // A small update keeps every unchanged chunk of the release it follows.
        writeUpdate(root / "source");
        content::ReleaseBuilder::Result update;
        report("Build an update that changed two files", [&] { update = builder.build(*io::Package::openDirectory(root / "source"), root / "update", root / "release", {.profile = "desktop", .appBuild = 1}); }, 0.0, "ms");
        printValue("Bytes the update stores beyond the release", static_cast<double>(update.content.statistics.storedBytes) / 1024.0, "KiB");
        printValue("Bytes of chunks the update reuses", static_cast<double>(update.content.statistics.reusedBytes) / kMegabyte, "MiB");

        report("Plan the ranges of 100000 records", [] { planRanges(); }, 0.0, "ms");
        report("Add 256 records of 256 KiB to the local store", [&] { addRecords(root / "store", compatibility.application); }, 256.0 * kRecordSize / kMegabyte, "MiB/s");

        // A release of tens of thousands of small files keeps opening and finding files cheap.
        writeManyFiles(root / "many");
        report("Build a release of 20000 files", [&] { (void)builder.build(*io::Package::openDirectory(root / "many"), root / "many-release", std::nullopt, {.profile = "desktop", .appBuild = 1}); }, 0.0, "ms");
        std::unique_ptr<content::ReleasePackage> many;
        report("Open the release of 20000 files", [&] { many = content::ReleasePackage::open(io::Package::openDirectory(root / "many-release"), keys, std::span(&trusted, 1), compatibility); }, 0.0, "ms");
        report("Find and read 20000 files in it", [&] { readManyFiles(*many); }, 0.0, "ms");

        std::filesystem::remove_all(root);
        return 0;
    }

  private:
    static constexpr const char* kIdentifier = "com.haylen.contentbenchmark";
    static constexpr double kMegabyte = 1024.0 * 1024.0;
    static constexpr std::uint64_t kLargeFile = std::uint64_t{64} << 20;
    static constexpr std::uint64_t kTextFile = std::uint64_t{16} << 20;
    static constexpr int kSmallFiles = 2000;
    static constexpr std::uint64_t kSmallFile = 2048;
    static constexpr std::size_t kRecordSize = 256 * 1024;
    static constexpr std::size_t kCodecChunk = 1024 * 1024;
    static constexpr std::size_t kModuleSize = 2 * 1024 * 1024;
    static constexpr int kManyFiles = 20000;
    static constexpr std::size_t kManyFileSize = 512;
    static constexpr int kImageSize = 1024;

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
        const std::string app = R"({"name": "Content Benchmark", "identifier": "com.haylen.contentbenchmark"})";
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
        const std::string module = makeModule();
        writeFile(folder / "source" / "world.lua", {module.begin(), module.end()});
        std::string map = R"({"width": 512, "height": 512, "layers": [{"name": "ground", "data": [)";
        for (int index = 0; index < 512 * 512; ++index) {
            map += (index > 0 ? "," : "") + std::to_string(1 + index % 37);
        }
        map += "]}]}";
        writeFile(folder / "content" / "maps" / "island.json", {map.begin(), map.end()});

        // A picture with gradients and a little grain, which compresses the way painted art does.
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kImageSize) * kImageSize * 4);
        std::mt19937 grain(5);
        for (int y = 0; y < kImageSize; ++y) {
            for (int x = 0; x < kImageSize; ++x) {
                const std::size_t pixel = (static_cast<std::size_t>(y) * kImageSize + static_cast<std::size_t>(x)) * 4;
                pixels[pixel] = static_cast<std::uint8_t>(x / 4 + grain() % 6);
                pixels[pixel + 1] = static_cast<std::uint8_t>(y / 4 + grain() % 6);
                pixels[pixel + 2] = static_cast<std::uint8_t>((x + y) / 8 + grain() % 6);
                pixels[pixel + 3] = 255;
            }
        }
        writeFile(folder / "content" / "images" / "scene.rgba", pixels);
        writeFile(folder / "content" / "images" / "scene.png", graphics::Image::fromRendered(pixels, kImageSize, kImageSize, false).encodePng());
    }

    static void printValue(const char* name, double value, const char* unit) {
        std::printf("%-52s %10.2f %-5s\n", name, value, unit);
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

    static void readOnThreads(const io::Package& package, const std::string& path) {
        std::vector<std::jthread> readers;
        const std::uint64_t quarter = kLargeFile / 4;
        for (std::uint64_t part = 0; part < 4; ++part) {
            // clang-format off
            readers.emplace_back([&package, &path, part, quarter] {
                const std::unique_ptr<io::PackageReader> reader = package.openReader(path);
                std::vector<std::uint8_t> buffer(1024 * 1024);
                for (std::uint64_t offset = part * quarter; offset < (part + 1) * quarter; offset += buffer.size()) {
                    (void)reader->read(offset, buffer);
                }
            });
            // clang-format on
        }
    }

    static std::vector<content::Compressor::Encoded> compressChunks(const std::vector<std::uint8_t>& text) {
        std::vector<content::Compressor::Encoded> chunks;
        for (std::size_t offset = 0; offset < text.size(); offset += kCodecChunk) {
            chunks.push_back(content::Compressor::encode(std::span(text).subspan(offset, std::min(kCodecChunk, text.size() - offset))));
        }
        return chunks;
    }

    static void decompressChunks(const std::vector<content::Compressor::Encoded>& chunks, std::size_t size) {
        std::vector<std::uint8_t> plain(kCodecChunk);
        std::size_t remaining = size;
        for (const content::Compressor::Encoded& chunk : chunks) {
            const std::size_t length = std::min(kCodecChunk, remaining);
            content::Compression::decode(chunk.codec, chunk.bytes, std::span(plain).first(length));
            remaining -= length;
        }
    }

    static void encryptChunks(const std::vector<std::uint8_t>& text) {
        const content::ContentKey key(makeKey(1));
        std::vector<std::uint8_t> cipher(kCodecChunk);
        std::vector<std::uint8_t> plain(kCodecChunk);
        for (std::size_t offset = 0; offset < text.size(); offset += kCodecChunk) {
            const std::span<const std::uint8_t> chunk = std::span(text).subspan(offset, std::min(kCodecChunk, text.size() - offset));
            const content::Aead::Nonce nonce = key.deriveNonce(content::ContentKey::Purpose::Chunk, chunk.first(64));
            const content::Aead::Tag tag = content::Aead::seal(key.getSubkey(content::ContentKey::Purpose::Chunk), nonce, {}, chunk, std::span(cipher).first(chunk.size()));
            if (!content::Aead::open(key.getSubkey(content::ContentKey::Purpose::Chunk), nonce, {}, tag, std::span(cipher).first(chunk.size()), std::span(plain).first(chunk.size()))) {
                std::fprintf(stderr, "A chunk did not decrypt.\n");
            }
        }
    }

    // A module of many small functions, as the code of a large game is.
    static std::string makeModule() {
        std::string module = "local world = {}\n";
        for (int index = 0; module.size() < kModuleSize; ++index) {
            module += "function world.rule" + std::to_string(index) + "(value, scale) local result = value * scale + " + std::to_string(index) + " if result > 100 then return result - 100 end return result end\n";
        }
        return module + "return world\n";
    }

    static void loadChunk(std::span<const std::uint8_t> chunk, const char* mode) {
        const std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(), &lua_close);
        if (luaL_loadbufferx(state.get(), reinterpret_cast<const char*>(chunk.data()), chunk.size(), "=world", mode) != LUA_OK) {
            std::fprintf(stderr, "The module did not load: %s\n", lua_tostring(state.get(), -1));
        }
    }

    // The bytes that the records of a file take in the release, compressed where compression pays.
    static std::uint64_t storedSize(const content::ReleasePackage& release, const std::string& path) {
        const std::shared_ptr<const content::ReleasePackage::Content> content = release.getContent();
        const content::Catalog& catalog = *content->catalog;
        const content::Catalog::File file = catalog.getFile(*catalog.findFile(path));
        std::uint64_t bytes = 0;
        for (std::uint64_t part = file.firstPart; part < file.firstPart + file.partCount; ++part) {
            bytes += content::ChunkRecord::kHeaderSize + catalog.getChunk(catalog.getPart(part).chunk).encodedSize;
        }
        return bytes;
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

    static void writeManyFiles(const std::filesystem::path& folder) {
        const std::string app = R"({"name": "Many Files", "identifier": "com.haylen.contentbenchmark"})";
        writeFile(folder / "app.json", {app.begin(), app.end()});
        writeFile(folder / "source" / "main.lua", {});
        for (int index = 0; index < kManyFiles; ++index) {
            writeFile(folder / "content" / "cards" / std::to_string(index % 100) / ("card-" + std::to_string(index) + ".json"), randomBytes(kManyFileSize, static_cast<std::uint32_t>(index + 50000)));
        }
    }

    static void readManyFiles(const io::Package& package) {
        std::size_t bytes = 0;
        for (int index = 0; index < kManyFiles; ++index) {
            bytes += package.read("content/cards/" + std::to_string(index % 100) + "/card-" + std::to_string(index) + ".json").size();
        }
        if (bytes != static_cast<std::size_t>(kManyFiles) * kManyFileSize) {
            std::fprintf(stderr, "The release lost files.\n");
        }
    }

    // The update changes a line in the middle of the text file and one small file, as a patch of a game does.
    static void writeUpdate(const std::filesystem::path& folder) {
        std::vector<std::uint8_t> text = readFile(folder / "content" / "data" / "text.txt");
        const std::string line = "The lighthouse keeper rewrote this line for the update.\n";
        std::ranges::copy(line, text.begin() + static_cast<std::ptrdiff_t>(text.size() / 2));
        writeFile(folder / "content" / "data" / "text.txt", text);
        writeFile(folder / "content" / "items" / "item_7.bin", randomBytes(kSmallFile, 900001));
    }

    static std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
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
            const content::ChunkRecord record = content::RecordSealer::seal(key, plain, content::ChunkRecord::identifyContent(plain), ciphertext);
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
