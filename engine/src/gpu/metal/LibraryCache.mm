#include "gpu/metal/LibraryCache.hpp"

#include <utility>

#include "haylen/core/JobSystem.hpp"

namespace haylen::gpu::metal {

std::shared_ptr<LibraryCache::Entry> LibraryCache::find(std::string_view source, bool& added) {
    const std::scoped_lock lock(mutex);
    auto [found, inserted] = entries.try_emplace(std::string(source));
    if (inserted) {
        found->second = std::make_shared<Entry>();
    }
    added = inserted;
    return found->second;
}

void LibraryCache::prepare(std::string_view source, core::JobSystem& jobs) {
    bool added = false;
    std::shared_ptr<Entry> entry = find(source, added);
    if (!added) {
        return;
    }

    // clang-format off
    jobs.postIo([metal = device, text = std::string(source), entry = std::move(entry)] {
        compile(metal, text, *entry);
    });
    // clang-format on
}

void LibraryCache::compile(id<MTLDevice> metal, std::string_view source, Entry& entry) {
    {
        const std::scoped_lock lock(entry.mutex);
        if (entry.state != State::Queued) {
            return;
        }
        entry.state = State::Compiling;
    }

    id<MTLLibrary> library = nil;
    std::string error;
    @autoreleasepool {
        NSString* text = [[NSString alloc] initWithBytes:source.data() length:source.size() encoding:NSUTF8StringEncoding];
        NSError* failure = nil;
        library = [metal newLibraryWithSource:text options:nil error:&failure];
        if (library == nil) {
            error = failure == nil ? "The compiler gave no reason." : failure.localizedDescription.UTF8String;
        }
    }

    {
        const std::scoped_lock lock(entry.mutex);
        entry.library = library;
        entry.error = std::move(error);
        entry.state = State::Compiled;
    }
    entry.compiled.notify_all();
}

id<MTLLibrary> LibraryCache::acquire(std::string_view source, std::string& error) {
    bool added = false;
    const std::shared_ptr<Entry> entry = find(source, added);
    compile(device, source, *entry);

    std::unique_lock lock(entry->mutex);
    entry->compiled.wait(lock, [&entry] { return entry->state == State::Compiled; });
    if (entry->library != nil) {
        ++entry->users;
        return entry->library;
    }
    error = entry->error;
    lock.unlock();

    // A source that failed leaves the cache, so the next attempt compiles it again.
    const std::scoped_lock cacheLock(mutex);
    if (const auto found = entries.find(std::string(source)); found != entries.end() && found->second == entry) {
        entries.erase(found);
    }
    return nil;
}

void LibraryCache::release(id<MTLLibrary> library) {
    const std::scoped_lock lock(mutex);
    for (auto found = entries.begin(); found != entries.end(); ++found) {
        bool unused = false;
        {
            const std::scoped_lock entryLock(found->second->mutex);
            if (found->second->library != library) {
                continue;
            }
            unused = --found->second->users <= 0;
        }
        if (unused) {
            entries.erase(found);
        }
        return;
    }
}

} // namespace haylen::gpu::metal
