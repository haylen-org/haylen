#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string_view>
#include <thread>

#include "gpu/direct3d/ShaderCompiler.hpp"

namespace haylen::gpu::direct3d {

// The bytecode of a compilation that a program never took lives in the system library of the compiler, so that library stays loaded while a compiler that goes away releases it.
TEST(ShaderCompilerTest, ReleasesTheBytecodeThatNoProgramTookWhileItsLibraryStays) {
    static constexpr std::string_view kSource = "float4 main() : SV_Position { return float4(0.0, 0.0, 0.0, 1.0); }";
    static constexpr std::chrono::seconds kCompileTime{2};
    for (int round = 0; round < 4; ++round) {
        auto compiler = std::make_shared<ShaderCompiler>(false);
        compiler->prepare({.vertex = {.source = kSource, .entry = "main", .profile = "vs_5_0"}});
        std::this_thread::sleep_for(kCompileTime);
        compiler.reset();
    }
    SUCCEED();
}

} // namespace haylen::gpu::direct3d
