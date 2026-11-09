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

// The Metal libraries of the stages of programs, loaded from the libraries that the build precompiled for the programs of the renderer and compiled from the source of every other program, such as a shader of the app. The method `prepare` starts making a library on the I/O pool, and programs take each library once and share it while one of them uses it. A program made while its library is in the making waits only for what is left, and one whose library has not started yet makes it on the frame thread at once instead of waiting for the pool. Every method may run on any thread.
class LibraryCache final {
  public:
    explicit LibraryCache(id<MTLDevice> metal) : device(metal) {}

    // Starts making the library of the source on the I/O pool, unless the cache holds it or makes it already.
    void prepare(std::string_view source, core::JobSystem& jobs);

    // The library of the source, which the caller uses until it calls `release` with it, or `nil` with the reason in `error` when it does not compile or its precompiled library does not load.
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

    // Makes the library of the source of the entry unless another thread took it first.
    static void compile(id<MTLDevice> metal, std::string_view source, Entry& entry);

    [[nodiscard]] std::shared_ptr<Entry> find(std::string_view source, bool& added);

    id<MTLDevice> device = nil;
    std::mutex mutex;
    std::unordered_map<std::string, std::shared_ptr<Entry>> entries;
};

} // namespace haylen::gpu::metal
