#pragma once

#include <atomic>

namespace haylen::platform {

// Platform code raises a memory warning from any thread, and the runtime hands it to the engine as a low memory event on its next frame.
class MemoryWarning final {
  public:
    static void raise() noexcept;
    [[nodiscard]] static bool take() noexcept;

  private:
    static std::atomic<bool> raised;
};

} // namespace haylen::platform
