#pragma once

#include <d3d11_1.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <optional>

namespace haylen::gpu::d3d11 {

// Measures the time the GPU takes for each frame with timestamp queries inside a disjoint query, which says whether the clock of the GPU stayed steady. It reads the queries of earlier frames only once the GPU finished them, so it never waits, and a frame that finds every query still pending goes unmeasured.
class FrameTimer final {
  public:
    explicit FrameTimer(ID3D11Device& device);

    void begin(ID3D11DeviceContext& context);
    void end(ID3D11DeviceContext& context);

    [[nodiscard]] std::optional<double> getSeconds() const noexcept {
        return seconds;
    }

  private:
    static constexpr std::size_t kSlots = 4;

    struct Slot {
        Microsoft::WRL::ComPtr<ID3D11Query> disjoint;
        Microsoft::WRL::ComPtr<ID3D11Query> start;
        Microsoft::WRL::ComPtr<ID3D11Query> finish;
        bool pending = false;
    };

    void collect(ID3D11DeviceContext& context);

    std::array<Slot, kSlots> slots;
    std::size_t current = 0;
    std::size_t oldest = 0;
    bool open = false;
    std::optional<double> seconds;
};

} // namespace haylen::gpu::d3d11
