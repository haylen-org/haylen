#pragma once

#include <dxgidebug.h>
#include <wrl/client.h>

#include <cstddef>
#include <string_view>
#include <vector>

#include "haylen/core/Log.hpp"

namespace haylen::gpu::direct3d {

// Moves what the debug layers of Direct3D and DXGI found into the log of the engine: corruptions and errors as errors, which fail the tests, and warnings as warnings. The queues of Direct3D 11 and 12 share their shape, so one template reads both. It keeps the storage of the messages, so a frame without new messages allocates nothing.
class DebugMessages final {
  public:
    // Takes the queue of DXGI, when its debug layer is present.
    DebugMessages();

    template <typename Queue, typename Message> void drain(Queue& queue, std::string_view api) {
        const UINT64 count = queue.GetNumStoredMessages();
        for (UINT64 index = 0; index < count; ++index) {
            SIZE_T size = 0;
            if (FAILED(queue.GetMessage(index, nullptr, &size))) {
                continue;
            }
            storage.resize(size);
            auto* message = reinterpret_cast<Message*>(storage.data());
            if (SUCCEEDED(queue.GetMessage(index, message, &size))) {
                write(static_cast<int>(message->Severity), api, std::string_view(message->pDescription, message->DescriptionByteLength > 0 ? message->DescriptionByteLength - 1 : 0));
            }
        }
        queue.ClearStoredMessages();
        drainDxgi();
    }

  private:
    void drainDxgi();
    static void write(int severity, std::string_view api, std::string_view description);

    Microsoft::WRL::ComPtr<IDXGIInfoQueue> dxgi;
    std::vector<std::byte> storage;
};

} // namespace haylen::gpu::direct3d
