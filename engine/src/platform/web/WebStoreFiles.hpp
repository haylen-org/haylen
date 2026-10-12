#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "content/FolderStoreFiles.hpp"
#include "content/StoreFiles.hpp"

namespace haylen::platform {

// The local content store of a web page, which the page keeps in the private file system of its origin through a worker, so downloads survive the next visit while the browser keeps the storage of the site. The small files, the state, the journal, manifests, shard indexes and staged records, also live in the memory file system, where the page restored them before the runtime started. Segments stay in the browser, and only the ranges that `load` reads in, the records that downloads append and the content an app needs to start live in memory, within a budget that drops the ranges read least recently. Every change reaches the worker in order, which writes and flushes it before the next one.
class WebStoreFiles final : public content::StoreFiles {
  public:
    static constexpr std::string_view kRoot = "/content-store";

    // The ranges of segments in memory beyond the pinned ones stay within this budget, past which the least recently used go.
    static constexpr std::uint64_t kResidentBudget = std::uint64_t{256} << 20;

    // A range used this recently stays even past the budget, so a range that a load just read in is there for the read that follows.
    static constexpr double kRecentMilliseconds = 5000.0;

    WebStoreFiles();
    ~WebStoreFiles() override;

    WebStoreFiles(const WebStoreFiles&) = delete;
    WebStoreFiles& operator=(const WebStoreFiles&) = delete;

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(const std::string& name) const override;
    void write(const std::string& name, std::span<const std::uint8_t> bytes) override;
    std::uint64_t append(const std::string& name, std::span<const std::uint8_t> bytes) override;
    void sync(const std::string& name) override;
    void truncate(const std::string& name, std::uint64_t size) override;
    void remove(const std::string& name) override;
    [[nodiscard]] std::uint64_t getSize(const std::string& name) const override;
    [[nodiscard]] std::vector<std::string> list(const std::string& folder) const override;
    void readRange(const std::string& name, std::uint64_t offset, std::span<std::uint8_t> target) const override;
    [[nodiscard]] std::uint64_t getAvailableSpace() const override;
    [[nodiscard]] bool isResident(const std::string& name, std::uint64_t offset, std::uint64_t size) const override;
    void load(std::vector<Range> ranges, std::function<void(bool)> done) override;
    void pin(const std::vector<Range>& ranges) override;

    // Receives a range that the page read from the browser, or none when the read failed.
    static void receive(std::uint32_t request, std::span<const std::uint8_t> bytes, bool ok);

  private:
    static constexpr std::string_view kSegmentFolder = "segments";
    static constexpr std::string_view kSizesFile = ".segments";

    struct Resident {
        std::vector<std::uint8_t> bytes;
        double lastUse = 0.0;
        bool pinned = false;
    };

    struct Load {
        std::size_t remaining = 0;
        bool ok = true;
        std::function<void(bool)> done;
    };

    struct Request {
        std::uint32_t load = 0;
        Range range;
    };

    [[nodiscard]] static bool isSegment(std::string_view name) noexcept;
    void keep(const std::string& name, std::uint64_t offset, std::vector<std::uint8_t> bytes);
    void evict();
    void finishRequest(std::uint32_t request, std::span<const std::uint8_t> bytes, bool ok);

    static WebStoreFiles* active;

    content::FolderStoreFiles metadata;
    std::map<std::string, std::uint64_t> segmentSizes;
    mutable std::map<std::pair<std::string, std::uint64_t>, Resident> resident;
    std::uint64_t residentBytes = 0;
    std::map<std::uint32_t, Load> loads;
    std::map<std::uint32_t, Request> requests;
    std::uint32_t nextId = 1;
};

} // namespace haylen::platform
