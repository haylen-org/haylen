#pragma once

#include <cstdint>
#include <mutex>

#include "haylen/platform/DialogResult.hpp"

namespace haylen::platform {

class Dialogs;

// Carries the answers of native dialogs to the dialogs of the running engine, which the platform plugin attaches when the app starts. The platform answers from any thread, so the lock keeps the dialogs alive until an answer has been queued, and an answer that arrives while no app runs is dropped.
class DialogRelay final {
  public:
    static void attach(Dialogs& value) noexcept;

    // Disconnects the relay when it still leads to these dialogs, so an engine that stops late never disconnects the one that replaced it.
    static void detach(const Dialogs& value) noexcept;

    static void resolve(std::uint64_t id, DialogResult result);

  private:
    static std::mutex& mutex;
    static Dialogs* dialogs;
};

} // namespace haylen::platform
