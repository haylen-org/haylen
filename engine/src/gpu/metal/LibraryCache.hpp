#pragma once

#import <Metal/Metal.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace haylen::core {
class JobSystem;
}

namespace haylen::gpu::metal {

// The Metal libraries compiled from the sources of the stages of programs, which `prepare` starts compiling on the I/O pool and which programs take once and share while one of them uses the library. A program made while its source compiles waits only for what is left, and one whose compile has not started yet compiles it on the frame thread at once instead of waiting for the pool. Every method may run on any thread.
class LibraryCache final {
  public:
    explicit LibraryCache(id<MTLDevice> metal) : device(metal) {}

    // Starts compiling the source on the I/O pool, unless the cache holds it or compiles it already.
    void prepare(std::string_view source, core::JobSystem& jobs);

    // The library of the source, which the caller uses until it calls `release` with it, or `nil` with the message of the compiler in `error` when the source does not compile.
    [[nodiscard]] id<MTLLibrary> acquire(std::string_view source, std::string& error);
    void release(id<MTLLibrary> library);

  private:
    enum class State : unsigned char {
        Queued,
        Compiling,
        Compiled,
    };

    struct Entry {
        std::mutex mutex;
        std::condition_variable compiled;
        State state = State::Queued;
        id<MTLLibrary> library = nil;
        std::string error;
        int users = 0;
    };

    // Compiles the source of the entry unless another thread took it first.
    static void compile(id<MTLDevice> metal, std::string_view source, Entry& entry);

    [[nodiscard]] std::shared_ptr<Entry> find(std::string_view source, bool& added);

    id<MTLDevice> device = nil;
    std::mutex mutex;
    std::unordered_map<std::string, std::shared_ptr<Entry>> entries;
};

} // namespace haylen::gpu::metal
