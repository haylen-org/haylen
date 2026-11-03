#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::platform {

// A capability that a platform may gate behind a permission, a prompt or a policy, such as the camera or the local network, and what the platform tells of it. Every platform answers every kind with a state of one vocabulary, so an app runs the same flow everywhere.
class Permission final {
  public:
    // The location is the one while the app is in use, at the precision the person allows. The precise location is the exact one, which a person may withhold while they allow an approximate one, and the background location is the one while the app is in the background.
    enum class Kind : std::uint8_t {
        Camera,
        Microphone,
        Location,
        PreciseLocation,
        BackgroundLocation,
        Notifications,
        Photos,
        Clipboard,
        Motion,
        Screens,
        LocalNetwork,
    };

    // The app holds the permission or the platform needs none, asking may show the prompt of the system, the person refused it or the system stopped asking, the platform does not tell the app until it tries, or the platform has no such capability, which the message of the answer explains.
    enum class State : std::uint8_t {
        Granted,
        Prompt,
        Denied,
        Unknown,
        Unsupported,
    };

    // What the project of the app lacks for a permission, such as the line of the Android manifest that declares it, with the file that holds it and the snippet that adds it.
    struct Requirement {
        std::string kind;
        std::string name;
        std::string file;
        std::string snippet;

        [[nodiscard]] bool operator==(const Requirement&) const = default;
    };

    // The answer of a platform: the state, and for `Unsupported` the sentence that says why, with the requirements the project lacks when that is the reason. The calls of capabilities that a platform may gate answer with it too: `Granted` once the call did its work, and `Denied` or `Unsupported` with the sentence that names the capability and what to do.
    struct Answer {
        State state = State::Unknown;
        std::vector<Requirement> missing;
        std::string message;

        // Reads the JSON object `{state, missing, message}` that native code answers with, where `missing` lists `{kind, name, file, snippet}`.
        [[nodiscard]] static Answer fromJson(const core::Json& value);
        [[nodiscard]] core::Json missingToJson() const;
    };

    [[nodiscard]] static std::string_view kindName(Kind value) noexcept;
    [[nodiscard]] static std::optional<Kind> kindFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view stateName(State value) noexcept;
    [[nodiscard]] static std::optional<State> stateFromName(std::string_view name) noexcept;

    // What the kind gives the app in words, such as `the camera`, for the sentences of answers.
    [[nodiscard]] static std::string_view describe(Kind value) noexcept;

    // The sentence of a use of the capability that the person refused, which names it and how to ask for it again.
    [[nodiscard]] static std::string refusal(Kind value);

  private:
    // Returns the text of a key of a JSON object, or an empty text when it holds something else.
    [[nodiscard]] static std::string readText(const core::Json& object, const char* key);

    static const std::array<std::pair<std::string_view, Kind>, 11> kKindNames;
    static const std::array<std::string_view, 11> kKindDescriptions;
    static const std::array<std::pair<std::string_view, State>, 5> kStateNames;
};

} // namespace haylen::platform
