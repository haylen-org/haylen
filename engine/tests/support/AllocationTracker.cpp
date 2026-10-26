#include "support/AllocationTracker.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <new>
#include <string_view>

#if defined(__APPLE__)
#include <dlfcn.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>

#include <array>
#elif defined(__linux__)
#include <dlfcn.h>
#include <execinfo.h>

#include <array>
#elif defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

namespace haylen::test {

thread_local AllocationTracker* AllocationTracker::active = nullptr;

AllocationTracker::AllocationTracker() noexcept : previous(active) {
    active = this;
}

AllocationTracker::~AllocationTracker() {
    active = previous;
}

void AllocationTracker::record(std::size_t size, const void* caller) noexcept {
#if defined(_MSC_VER) && _ITERATOR_DEBUG_LEVEL != 0
    // The checked iterators of the debug library of MSVC give every container a proxy that the library allocates on its own, so the tracker leaves out the allocations of the size of that proxy there, while the other libraries count every size.
    if (size == sizeof(std::_Container_proxy)) {
        return;
    }
#endif
    if (active == nullptr || isSystemGraphics(caller)) {
        return;
    }
    active->largest = std::max(active->largest, size);
    active->total += size;
}

// The GPU drivers of Apple platforms live in the extensions of the system, and Metal validates through the private framework of its tools. On the web the WebGPU of the browser comes through its port, which makes a handle in the memory of the program for every command encoder, pass and command buffer, and the stack of an allocation names the functions of that port, which start with "wgpu" or "emwgpu". On Linux the Vulkan loader, its layers and the drivers, with the compiler that software drivers build their shaders with, are libraries of the system that allocate through the allocation functions of the program.
bool AllocationTracker::isSystemGraphics(const void* caller) noexcept {
#if defined(__EMSCRIPTEN__)
    static_cast<void>(caller);
    std::array<char, kStackBytes> stack{};
    emscripten_get_callstack(EM_LOG_JS_STACK, stack.data(), static_cast<int>(stack.size()));
    const std::string_view frames(stack.data());
    for (std::size_t found = frames.find("wgpu"); found != std::string_view::npos; found = frames.find("wgpu", found + 1)) {
        const std::size_t next = found + 4;
        if (next < frames.size() && frames[next] >= 'A' && frames[next] <= 'Z') {
            return true;
        }
    }
    return false;
#elif defined(__APPLE__)
    Dl_info image{};
    if (dladdr(caller, &image) == 0 || image.dli_fname == nullptr) {
        return false;
    }
    const std::string_view path(image.dli_fname);
    return path.starts_with("/System/Library/Extensions/") || path.starts_with("/System/Library/PrivateFrameworks/MetalTools.framework/");
#elif defined(__linux__)
    return isLinuxGraphics(caller);
#else
    static_cast<void>(caller);
    return false;
#endif
}

#if defined(__linux__)
// The libraries of Vulkan often allocate through the functions of the C++ library, which the program may hold itself, so the code that allocates is the first caller outside the program and the C++ library, while the allocations of the engine reach the start of their thread first.
bool AllocationTracker::isLinuxGraphics(const void* caller) noexcept {
    std::array<void*, kStackDepth> frames{};
    const int depth = backtrace(frames.data(), static_cast<int>(frames.size()));
    const std::string_view program = getLibraryName(reinterpret_cast<const void*>(&AllocationTracker::record));
    const auto start = std::find(frames.begin(), frames.begin() + depth, caller);
    for (auto frame = start; frame != frames.begin() + depth; ++frame) {
        const std::string_view name = getLibraryName(*frame);
        if (name == program || name.starts_with("libstdc++") || name.starts_with("libc++")) {
            continue;
        }
        return name.starts_with("libvulkan") || name.starts_with("libVkLayer_") || name.starts_with("libnvidia-") || name.starts_with("libLLVM");
    }
    return false;
}

std::string_view AllocationTracker::getLibraryName(const void* address) noexcept {
    Dl_info image{};
    if (dladdr(address, &image) == 0 || image.dli_fname == nullptr) {
        return {};
    }
    const std::string_view path(image.dli_fname);
    return path.substr(path.find_last_of('/') + 1);
}
#endif

} // namespace haylen::test

// The standard names the replaceable allocation functions, so the program replaces them here, and they only forward to the tracker, with the address of the code that allocates, and to `malloc` and `free`. The array and nothrow forms are replaced too, since a sanitizer that replaces the forms a program leaves out would free its own allocations with the `free` of these, while the aligned forms keep their own pair.
void* operator new(std::size_t size) {
#if defined(_MSC_VER) && !defined(__clang__)
    haylen::test::AllocationTracker::record(size, _ReturnAddress());
#elif defined(__EMSCRIPTEN__)
    haylen::test::AllocationTracker::record(size, nullptr);
#else
    haylen::test::AllocationTracker::record(size, __builtin_return_address(0));
#endif
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    return operator new(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
#if defined(_MSC_VER) && !defined(__clang__)
    haylen::test::AllocationTracker::record(size, _ReturnAddress());
#elif defined(__EMSCRIPTEN__)
    haylen::test::AllocationTracker::record(size, nullptr);
#else
    haylen::test::AllocationTracker::record(size, __builtin_return_address(0));
#endif
    return std::malloc(size == 0 ? 1 : size);
}

void* operator new[](std::size_t size, const std::nothrow_t& tag) noexcept {
    return operator new(size, tag);
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void* memory) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete(void* memory, const std::nothrow_t&) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, const std::nothrow_t&) noexcept {
    std::free(memory);
}
