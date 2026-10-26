#pragma once

#include <d3dcommon.h>
#include <d3dcompiler.h>
#include <windows.h>
#include <wrl/client.h>

#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "haylen/gpu/Device.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::gpu::direct3d {

// Compiles the HLSL of shader packages into the bytecode of shader model 5.0, which Direct3D 11 and 12 both run, through `D3DCompile` of the system library `d3dcompiler_47.dll`, which it loads when it starts. Programs prepared ahead compile on the I/O pool, and the frame thread takes their bytecode when it makes the program, waiting only for what is left. Every method may run on any thread.
class ShaderCompiler final : public std::enable_shared_from_this<ShaderCompiler> {
  public:
    // Loads the compiler, which keeps debug information in the bytecode for GPU debuggers when `debug` is on. Throws `std::runtime_error` when the system lacks it.
    explicit ShaderCompiler(bool debug);
    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;
    ~ShaderCompiler();

    // The bytecode of a stage, which the stage brings itself or the compiler makes from its source. Throws `std::runtime_error` with the messages of the compiler when the source does not compile.
    [[nodiscard]] Microsoft::WRL::ComPtr<ID3DBlob> compile(const Device::StageCode& stage, std::string_view label);

    // Starts compiling both stages of the program on the I/O pool, unless they compile already.
    void prepare(const Device::ProgramDesc& desc, core::JobSystem& jobs);

  private:
    struct Result {
        Microsoft::WRL::ComPtr<ID3DBlob> code;
        std::string error;
    };

    [[nodiscard]] static std::string makeKey(const Device::StageCode& stage);
    [[nodiscard]] Result run(const std::string& key) const;
    [[nodiscard]] Result take(const std::shared_future<Result>& prepared, const std::string& key) const;
    void start(const Device::StageCode& stage, core::JobSystem& jobs);

    using CreateBlob = HRESULT(WINAPI*)(SIZE_T, ID3DBlob**);

    HMODULE library = nullptr;
    pD3DCompile function = nullptr;
    CreateBlob createBlob = nullptr;
    UINT flags = 0;
    std::mutex mutex;
    // The compilations that `prepare` started, under the profile, entry point and source of the stage they compile, until a program takes them.
    std::unordered_map<std::string, std::shared_future<Result>> started;
};

} // namespace haylen::gpu::direct3d
