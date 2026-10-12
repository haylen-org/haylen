#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace haylen::content {

// The files of a local content store by their names, which use `/` between folders. Native platforms keep them in a folder of the device, and the web in the private file system of the origin. Reads of ranges may run on several threads at once, and the store makes every other call from one thread at a time. Every call throws `content::Error` when the device refuses it: `DiskFull`, `QuotaExceeded`, `PermissionDenied` or `CacheCorrupt`.
class StoreFiles {
  public:
    struct Range {
        std::string name;
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
    };

    virtual ~StoreFiles() = default;

    // Reads a whole small file, or returns nothing when it does not exist.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> read(const std::string& name) const = 0;

    // Replaces a file with the bytes, durably once the call returns.
    virtual void write(const std::string& name, std::span<const std::uint8_t> bytes) = 0;

    // Adds the bytes to the end of a file, which it creates when it is missing, and returns the offset where they start. They are durable only once `sync` returns.
    virtual std::uint64_t append(const std::string& name, std::span<const std::uint8_t> bytes) = 0;
    virtual void sync(const std::string& name) = 0;
    virtual void truncate(const std::string& name, std::uint64_t size) = 0;
    virtual void remove(const std::string& name) = 0;

    // The size of a file, zero when it does not exist.
    [[nodiscard]] virtual std::uint64_t getSize(const std::string& name) const = 0;

    // The names of the files in a folder.
    [[nodiscard]] virtual std::vector<std::string> list(const std::string& folder) const = 0;

    // Fills the whole target from an offset of a file, and throws `CacheCorrupt` when the file ends first.
    virtual void readRange(const std::string& name, std::uint64_t offset, std::span<std::uint8_t> target) const = 0;

    // The bytes that the store may still add before the device or the browser refuses them.
    [[nodiscard]] virtual std::uint64_t getAvailableSpace() const = 0;

    // Whether the bytes of a range can be read now. Files on the device always can, while the web keeps them in the browser and reads in only the ranges that `load` asks for.
    [[nodiscard]] virtual bool isResident(const std::string& name, std::uint64_t offset, std::uint64_t size) const;

    // Reads ranges in so they can be read, and calls back with whether all of them came in, at once on the device and later on the web, on the thread that runs the page.
    virtual void load(std::vector<Range> ranges, std::function<void(bool)> done);

    // Keeps ranges readable for the whole run, which the content an app needs to start asks for.
    virtual void pin(const std::vector<Range>& ranges);
};

} // namespace haylen::content
