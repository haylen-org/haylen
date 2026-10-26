// The implementation of the Vulkan Memory Allocator, which compiles in a library of its own without the warnings of the engine and finds the functions of Vulkan through the loader that volk opens.
#include <volk.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
