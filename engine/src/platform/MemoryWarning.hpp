#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

#include "haylen/platform/Event.hpp"

namespace haylen::platform {

// Platform code raises a memory warning from any thread, and the runtime hands it to the engine as a low memory event on its next frame, with the highest level raised since the frame before.
class MemoryWarning final {
  public:
    static void raise(Event::MemoryLevel level) noexcept;
    [[nodiscard]] static std::optional<Event::MemoryLevel> take() noexcept;

  private:
    static constexpr std::uint8_t kNone = 0;

    // The level raised last plus one, or `kNone`.
    static std::atomic<std::uint8_t> raised;
};

} // namespace haylen::platform
