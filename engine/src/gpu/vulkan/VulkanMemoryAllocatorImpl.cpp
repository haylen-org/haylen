// The implementation of the Vulkan Memory Allocator, which compiles in a library of its own without the warnings of the engine and finds the functions of Vulkan through the loader that volk opens. On Windows it shares memory through handles of the system, whose header volk leaves out.
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <volk.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
