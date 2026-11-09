#pragma once

#include <optional>
#include <string>
#include <unordered_set>

#include "haylen/platform/Permission.hpp"

namespace haylen::core {
class Engine;
}

namespace haylen::lua {
class Promise;
}

namespace haylen::platform {

// Settles the promises of the calls that a platform may gate, such as the clipboard of `haylen.system` and the fullscreen of `haylen.window`, with a `Permission::Answer`, on the frame thread. A refusal rejects with the sentence in `error`, the code `denied` or `unsupported`, and the requirements that the project lacks in `data.missing`, and the app asked, so a rejection that nothing awaits never stops it.
class AnswerLua final {
  public:
    // Resolves with the name of the state, and rejects an answer of `unsupported`.
    static void settlePermission(core::Engine& engine, const lua::Promise& promise, const Permission::Answer& answer);

    // Resolves with the value on `granted`, or with nothing without one, and rejects anything else, which the engine logs once per sentence, so a call whose promise nothing awaits, such as a copy to the clipboard that a browser refused, still tells the developer why.
    static void settle(core::Engine& engine, const lua::Promise& promise, const Permission::Answer& answer, std::optional<std::string> value = std::nullopt);

  private:
    static void reject(core::Engine& engine, const lua::Promise& promise, const Permission::Answer& answer);

    // The sentences of the refused calls the engine logged, once each.
    static std::unordered_set<std::string> loggedRefusals;
};

} // namespace haylen::platform
