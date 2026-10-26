#include "gpu/direct3d/ShaderCompiler.hpp"

#include <cstring>
#include <format>
#include <stdexcept>
#include <utility>

namespace haylen::gpu::direct3d {

using Microsoft::WRL::ComPtr;

ShaderCompiler::ShaderCompiler(bool debug) : library(getLibrary()), flags(debug ? D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION : D3DCOMPILE_OPTIMIZATION_LEVEL3) {}

// A library that fails to load leaves nothing behind, so the next compiler tries again.
const ShaderCompiler::Library& ShaderCompiler::getLibrary() {
    static const Library* const loaded = loadLibrary();
    return *loaded;
}

const ShaderCompiler::Library* ShaderCompiler::loadLibrary() {
    const HMODULE handle = LoadLibraryExW(L"d3dcompiler_47.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (handle == nullptr) {
        throw std::runtime_error("The shader compiler of Direct3D, \"d3dcompiler_47.dll\", is missing from the system.");
    }
    const auto compileFunction = reinterpret_cast<pD3DCompile>(reinterpret_cast<void*>(GetProcAddress(handle, "D3DCompile")));
    const auto blobFunction = reinterpret_cast<CreateBlob>(reinterpret_cast<void*>(GetProcAddress(handle, "D3DCreateBlob")));
    if (compileFunction == nullptr || blobFunction == nullptr) {
        FreeLibrary(handle);
        throw std::runtime_error("The shader compiler of Direct3D, \"d3dcompiler_47.dll\", lacks the functions \"D3DCompile\" and \"D3DCreateBlob\".");
    }
    return new Library{.compile = compileFunction, .createBlob = blobFunction};
}

// The key holds the profile and the entry point before the source, each ending in a null character, so `run` reads all three back from it.
std::string ShaderCompiler::makeKey(const Device::StageCode& stage) {
    std::string key;
    key.reserve(stage.profile.size() + stage.entry.size() + stage.source.size() + 2);
    key.append(stage.profile).push_back('\0');
    key.append(stage.entry).push_back('\0');
    key.append(stage.source);
    return key;
}

ComPtr<ID3DBlob> ShaderCompiler::compile(const Device::StageCode& stage, std::string_view label) {
    if (!stage.bytecode.empty()) {
        ComPtr<ID3DBlob> code;
        if (FAILED(library.createBlob(stage.bytecode.size(), code.GetAddressOf()))) {
            throw std::runtime_error(std::format("The graphics device found no memory for the bytecode of the shader \"{}\".", label));
        }
        std::memcpy(code->GetBufferPointer(), stage.bytecode.data(), stage.bytecode.size());
        return code;
    }

    const std::string key = makeKey(stage);
    std::shared_future<Result> prepared;
    {
        const std::scoped_lock lock(mutex);
        const auto found = started.find(key);
        if (found != started.end()) {
            prepared = std::move(found->second);
            started.erase(found);
        }
    }

    Result result = prepared.valid() ? take(prepared, key) : run(key);
    if (result.code == nullptr) {
        throw std::runtime_error(std::format("The shader \"{}\" does not compile for Direct3D: {}", label, result.error));
    }
    return std::move(result.code);
}

// A compilation whose job the pools dropped before it ran, as they do when the engine stops, compiles here instead.
ShaderCompiler::Result ShaderCompiler::take(const std::shared_future<Result>& prepared, const std::string& key) const {
    try {
        return prepared.get();
    } catch (const std::future_error&) {
        return run(key);
    }
}

ShaderCompiler::Result ShaderCompiler::run(const std::string& key) const {
    const std::size_t entryStart = key.find('\0') + 1;
    const std::size_t sourceStart = key.find('\0', entryStart) + 1;
    const char* profile = key.c_str();
    const char* entry = key.c_str() + entryStart;
    const char* source = key.c_str() + sourceStart;

    Result result;
    ComPtr<ID3DBlob> errors;
    const HRESULT status = library.compile(source, key.size() - sourceStart, nullptr, nullptr, nullptr, entry, profile, flags, 0, result.code.GetAddressOf(), errors.GetAddressOf());
    if (SUCCEEDED(status)) {
        return result;
    }
    result.code.Reset();
    result.error = errors != nullptr ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::format("the compiler failed with the error 0x{:08X}.", static_cast<unsigned long>(status));
    return result;
}

void ShaderCompiler::prepare(const Device::ProgramDesc& desc) {
    start(desc.vertex);
    start(desc.fragment);
}

// A compilation holds the compiler only while it runs, so the device that lets the compiler go drops the compilations that wait.
void ShaderCompiler::start(const Device::StageCode& stage) {
    if (!stage.bytecode.empty() || stage.source.empty()) {
        return;
    }
    std::string key = makeKey(stage);
    auto task = std::make_shared<std::packaged_task<Result()>>();
    {
        const std::scoped_lock lock(mutex);
        if (started.contains(key)) {
            return;
        }
        // clang-format off
        *task = std::packaged_task<Result()>([weak = weak_from_this(), key] {
            const std::shared_ptr<ShaderCompiler> compiler = weak.lock();
            return compiler != nullptr ? compiler->run(key) : Result{};
        });
        // clang-format on
        started.emplace(std::move(key), task->get_future().share());
    }
    queue.post([task] { (*task)(); });
}

} // namespace haylen::gpu::direct3d
