#include "gpu/direct3d/DebugMessages.hpp"

namespace haylen::gpu::direct3d {

DebugMessages::DebugMessages() {
    const HMODULE library = GetModuleHandleW(L"dxgidebug.dll");
    if (library == nullptr) {
        return;
    }
    using GetInterface = HRESULT(WINAPI*)(UINT, REFIID, void**);
    const auto get = reinterpret_cast<GetInterface>(reinterpret_cast<void*>(GetProcAddress(library, "DXGIGetDebugInterface1")));
    if (get != nullptr) {
        static_cast<void>(get(0, IID_PPV_ARGS(dxgi.GetAddressOf())));
    }
}

void DebugMessages::drainDxgi() {
    if (dxgi == nullptr) {
        return;
    }
    const UINT64 count = dxgi->GetNumStoredMessages(DXGI_DEBUG_ALL);
    for (UINT64 index = 0; index < count; ++index) {
        SIZE_T size = 0;
        if (FAILED(dxgi->GetMessage(DXGI_DEBUG_ALL, index, nullptr, &size))) {
            continue;
        }
        storage.resize(size);
        auto* message = reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE*>(storage.data());
        if (SUCCEEDED(dxgi->GetMessage(DXGI_DEBUG_ALL, index, message, &size))) {
            write(static_cast<int>(message->Severity), "DXGI", std::string_view(message->pDescription, message->DescriptionByteLength > 0 ? message->DescriptionByteLength - 1 : 0));
        }
    }
    dxgi->ClearStoredMessages(DXGI_DEBUG_ALL);
}

// The severities of every queue count from corruption, error and warning down to information and plain messages, which stay out of the log. A warning of the debug layer names a use of the API that the engine never means to make, such as a draw whose scissor covers no pixel, so it is an error of the engine too, and the tests fail on it.
void DebugMessages::write(int severity, std::string_view api, std::string_view description) {
    if (severity <= 1) {
        core::Log::error("The debug layer of {} found an error: {}", api, description);
    } else if (severity == 2) {
        core::Log::error("The debug layer of {} warns: {}", api, description);
    }
}

} // namespace haylen::gpu::direct3d
