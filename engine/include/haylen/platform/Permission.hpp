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

// A capability that a platform may gate behind a permission, a prompt or a policy, such as the local network, and what the platform tells of it. Every permission answers with a state of one vocabulary, so an app runs the same flow everywhere. The kinds are the permissions that the engine uses itself, which the host answers, while every other permission belongs to the plugin of the app that declares it in the `permissions` of its `plugin.json`, which answers it through the bridge.
class Permission final {
  public:
    enum class Kind : std::uint8_t {
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

    // The sentence of a use of the capability that the person refused, which names it and how to ask for it again, for the capability and the name of a permission, such as `the camera` and `camera`.
    [[nodiscard]] static std::string refusal(std::string_view capability, std::string_view name);

    // The sentence of a name that neither the engine nor a plugin of the app owns, which names the permissions of the engine and where a plugin declares its own.
    [[nodiscard]] static std::string unknown(std::string_view name);

    // Whether the name can name a permission of a plugin: a `camelCase` word of letters and digits that starts with a lowercase letter and is no kind of the engine.
    [[nodiscard]] static bool isPluginName(std::string_view name) noexcept;

  private:
    // Returns the text of a key of a JSON object, or an empty text when it holds something else.
    [[nodiscard]] static std::string readText(const core::Json& object, const char* key);

    static const std::array<std::pair<std::string_view, Kind>, 2> kKindNames;
    static const std::array<std::pair<std::string_view, State>, 5> kStateNames;
};

} // namespace haylen::platform
