#include "platform/web/WebStoreFiles.hpp"

#include <emscripten/emscripten.h>

#include <algorithm>
#include <utility>

#include "haylen/content/Error.hpp"
#include "haylen/core/Json.hpp"
#include "platform/web/WebEntry.hpp"

// The page side lives in `platform/web/haylen-runtime.js`, whose store hands every change to the worker of `haylen-content-store.js`.

// clang-format off
EM_JS(void, haylen_js_store_write, (const char* name, const uint8_t* data, int size), {
    Module.haylen.contentStore.write(UTF8ToString(name), HEAPU8.slice(data, data + size));
});

EM_JS(void, haylen_js_store_append, (const char* name, const uint8_t* data, int size), {
    Module.haylen.contentStore.append(UTF8ToString(name), HEAPU8.slice(data, data + size));
});

EM_JS(void, haylen_js_store_flush, (const char* name), {
    Module.haylen.contentStore.flush(UTF8ToString(name));
});

EM_JS(void, haylen_js_store_truncate, (const char* name, double size), {
    Module.haylen.contentStore.truncate(UTF8ToString(name), size);
});

EM_JS(void, haylen_js_store_remove, (const char* name), {
    Module.haylen.contentStore.remove(UTF8ToString(name));
});

EM_JS(void, haylen_js_store_read, (int request, const char* name, double offset, double size), {
    Module.haylen.contentStore.read(request, UTF8ToString(name), offset, size);
});

EM_JS(double, haylen_js_store_unwritten, (), {
    return Module.haylen.contentStore.unwritten();
});

EM_JS(double, haylen_js_store_available, (), {
    return Module.haylen.contentStore.available();
});
// clang-format on

namespace haylen::platform {

WebStoreFiles* WebStoreFiles::active = nullptr;

WebStoreFiles::WebStoreFiles() : metadata(std::filesystem::path(kRoot)) {
    active = this;
    if (const std::optional<std::vector<std::uint8_t>> bytes = metadata.read(std::string(kSizesFile))) {
        const core::Json sizes = core::Json::parse(*bytes);
        for (const auto& [name, size] : sizes.items()) {
            segmentSizes.emplace(name, size.get<std::uint64_t>());
        }
    }
}

WebStoreFiles::~WebStoreFiles() {
    active = nullptr;
}

bool WebStoreFiles::isSegment(std::string_view name) noexcept {
    return name.starts_with(kSegmentFolder) && name.size() > kSegmentFolder.size() && name[kSegmentFolder.size()] == '/';
}

std::optional<std::vector<std::uint8_t>> WebStoreFiles::read(const std::string& name) const {
    return metadata.read(name);
}

void WebStoreFiles::requireKept() const {
    if (refused) {
        throw *refused;
    }
}

void WebStoreFiles::write(const std::string& name, std::span<const std::uint8_t> bytes) {
    requireKept();
    metadata.write(name, bytes);
    haylen_js_store_write(name.c_str(), bytes.data(), static_cast<int>(bytes.size()));
}

std::uint64_t WebStoreFiles::append(const std::string& name, std::span<const std::uint8_t> bytes) {
    requireKept();
    haylen_js_store_append(name.c_str(), bytes.data(), static_cast<int>(bytes.size()));
    if (!isSegment(name)) {
        return metadata.append(name, bytes);
    }

    // A record that a download appends stays in memory while the budget has room for it, and goes first past it.
    std::uint64_t& size = segmentSizes[name];
    const std::uint64_t offset = size;
    size += bytes.size();
    keep(name, offset, {bytes.begin(), bytes.end()}, true);
    return offset;
}

// The worker writes every change before the next one and flushes a segment when the store syncs it, so the records reach the disk before the state that names them, and a refused change surfaces at the next one.
void WebStoreFiles::sync(const std::string& name) {
    requireKept();
    if (isSegment(name)) {
        haylen_js_store_flush(name.c_str());
    }
}

void WebStoreFiles::truncate(const std::string& name, std::uint64_t size) {
    requireKept();
    metadata.truncate(name, size);
    haylen_js_store_truncate(name.c_str(), static_cast<double>(size));
}

void WebStoreFiles::remove(const std::string& name) {
    requireKept();
    haylen_js_store_remove(name.c_str());
    if (!isSegment(name)) {
        metadata.remove(name);
        return;
    }
    segmentSizes.erase(name);
    for (auto entry = resident.begin(); entry != resident.end();) {
        if (entry->first.first == name) {
            residentBytes -= entry->second.bytes.size();
            entry = resident.erase(entry);
        } else {
            ++entry;
        }
    }
}

std::uint64_t WebStoreFiles::getSize(const std::string& name) const {
    if (!isSegment(name)) {
        return metadata.getSize(name);
    }
    const auto found = segmentSizes.find(name);
    return found == segmentSizes.end() ? 0 : found->second;
}

std::vector<std::string> WebStoreFiles::list(const std::string& folder) const {
    if (folder != kSegmentFolder) {
        return metadata.list(folder);
    }
    std::vector<std::string> names;
    for (const auto& [name, size] : segmentSizes) {
        names.push_back(name.substr(kSegmentFolder.size() + 1));
    }
    return names;
}

void WebStoreFiles::readRange(const std::string& name, std::uint64_t offset, std::span<std::uint8_t> target) const {
    if (!isSegment(name)) {
        metadata.readRange(name, offset, target);
        return;
    }
    const auto found = resident.find({name, offset});
    if (found == resident.end() || found->second.bytes.size() < target.size()) {
        throw content::Error(content::Error::Code::CacheCorrupt, "The content store did not read in the bytes " + std::to_string(offset) + " of \"" + name + "\" before they were read.");
    }
    found->second.lastUse = emscripten_get_now();
    found->second.appended = false;
    std::copy_n(found->second.bytes.begin(), target.size(), target.begin());
}

std::uint64_t WebStoreFiles::getAvailableSpace() const {
    return static_cast<std::uint64_t>(haylen_js_store_available());
}

bool WebStoreFiles::isResident(const std::string& name, std::uint64_t offset, std::uint64_t size) const {
    if (!isSegment(name)) {
        return true;
    }
    const auto found = resident.find({name, offset});
    return found != resident.end() && found->second.bytes.size() >= size;
}

void WebStoreFiles::load(std::vector<Range> ranges, std::function<void(bool)> done) {
    std::erase_if(ranges, [this](const Range& range) { return isResident(range.name, range.offset, range.size); });
    if (ranges.empty()) {
        done(true);
        return;
    }
    const std::uint32_t load = nextId++;
    loads.emplace(load, Load{.remaining = ranges.size(), .done = std::move(done)});
    for (Range& range : ranges) {
        const std::uint32_t request = nextId++;
        haylen_js_store_read(static_cast<int>(request), range.name.c_str(), static_cast<double>(range.offset), static_cast<double>(range.size));
        requests.emplace(request, Request{.load = load, .range = std::move(range)});
    }
}

std::uint64_t WebStoreFiles::getUnwrittenBytes() const {
    return static_cast<std::uint64_t>(haylen_js_store_unwritten());
}

void WebStoreFiles::pin(const std::vector<Range>& ranges) {
    for (const Range& range : ranges) {
        if (const auto found = resident.find({range.name, range.offset}); found != resident.end()) {
            found->second.pinned = true;
        }
    }
}

void WebStoreFiles::receive(std::uint32_t request, std::span<const std::uint8_t> bytes, bool ok) {
    if (active != nullptr) {
        active->finishRequest(request, bytes, ok);
    }
}

void WebStoreFiles::refuse(bool quota, std::string message) {
    if (active == nullptr || active->refused) {
        return;
    }
    const content::Error::Code code = quota ? content::Error::Code::QuotaExceeded : content::Error::Code::PermissionDenied;
    const std::string reason = quota ? "The browser ran out of storage for this site, so the downloaded content stays as it was before the last change." : "The browser refused a change of the downloaded content, which stays as it was before that change.";
    active->refused.emplace(code, reason, content::Error::Details{.platformMessage = std::move(message)});
}

void WebStoreFiles::finishRequest(std::uint32_t request, std::span<const std::uint8_t> bytes, bool ok) {
    const auto found = requests.find(request);
    if (found == requests.end()) {
        return;
    }
    const Request finished = std::move(found->second);
    requests.erase(found);
    Load& load = loads.at(finished.load);
    if (ok && bytes.size() == finished.range.size) {
        keep(finished.range.name, finished.range.offset, {bytes.begin(), bytes.end()}, false);
    } else {
        load.ok = false;
    }
    if (--load.remaining == 0) {
        const std::function<void(bool)> done = std::move(load.done);
        const bool all = load.ok;
        loads.erase(finished.load);
        done(all);
    }
}

void WebStoreFiles::keep(const std::string& name, std::uint64_t offset, std::vector<std::uint8_t> bytes, bool appended) {
    residentBytes += bytes.size();
    Resident& entry = resident[{name, offset}];
    residentBytes -= entry.bytes.size();
    entry.bytes = std::move(bytes);
    entry.lastUse = emscripten_get_now();
    entry.appended = appended;
    evict();
}

void WebStoreFiles::evict() {
    // The ranges used least recently go first, never a pinned one and never one read a moment ago, while a record that a download appended and nothing read yet may go at once.
    const double now = emscripten_get_now();
    while (residentBytes > kResidentBudget) {
        auto oldest = resident.end();
        for (auto entry = resident.begin(); entry != resident.end(); ++entry) {
            const bool evictable = entry->second.appended || now - entry->second.lastUse > kRecentMilliseconds;
            if (!entry->second.pinned && evictable && (oldest == resident.end() || entry->second.lastUse < oldest->second.lastUse)) {
                oldest = entry;
            }
        }
        if (oldest == resident.end()) {
            return;
        }
        residentBytes -= oldest->second.bytes.size();
        resident.erase(oldest);
    }
}

} // namespace haylen::platform

extern "C" {

EMSCRIPTEN_KEEPALIVE void haylen_web_store_received(int request, const std::uint8_t* bytes, int size, int ok) {
    haylen::platform::WebEntry::guard("haylen_web_store_received", [&] { haylen::platform::WebStoreFiles::receive(static_cast<std::uint32_t>(request), {bytes, static_cast<std::size_t>(size)}, ok != 0); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_store_refused(int quota, const char* message) {
    haylen::platform::WebEntry::guard("haylen_web_store_refused", [&] { haylen::platform::WebStoreFiles::refuse(quota != 0, message); });
}
}
