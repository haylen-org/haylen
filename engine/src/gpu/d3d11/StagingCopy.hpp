#pragma once

#include <d3d11_1.h>
#include <wrl/client.h>

#include <optional>
#include <string_view>

namespace haylen::gpu::d3d11 {

// A copy into a staging resource behind an event query, which maps the resource only once the query says the GPU finished the copy, so it never waits. The readbacks of render targets and buffers read through it.
class StagingCopy final {
  public:
    // Ends the copy into the resource that the context recorded last with an event query. Throws `std::runtime_error` when the device makes no query.
    StagingCopy(ID3D11Device& device, Microsoft::WRL::ComPtr<ID3D11DeviceContext> owner, Microsoft::WRL::ComPtr<ID3D11Resource> copy);

    // Maps the resource for reading once the GPU finished the copy, or returns nothing while it runs. Throws `std::runtime_error` with the failure when the GPU hands nothing over, such as after it stopped working.
    [[nodiscard]] std::optional<D3D11_MAPPED_SUBRESOURCE> map(std::string_view failure);
    void unmap() noexcept;

  private:
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<ID3D11Resource> staging;
    Microsoft::WRL::ComPtr<ID3D11Query> done;
};

} // namespace haylen::gpu::d3d11
