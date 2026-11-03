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

// A permission that the person grants the app while it runs, such as the access to the devices of the local network that Android asks for from Android 17, and what the platform tells of it.
class Permission final {
  public:
    enum class Kind : std::uint8_t {
        LocalNetwork,
    };

    // The app holds the permission or the platform needs none, asking may show the prompt of the system, the person refused it so asking answers at once, or the platform cannot tell, such as iOS, which asks the person the first time the app reaches the local network.
    enum class State : std::uint8_t {
        Granted,
        Prompt,
        Denied,
        Unknown,
    };

    // What the project of the app lacks for a permission, such as the line of the Android manifest that declares it, with the file that holds it and the snippet that adds it.
    struct Requirement {
        std::string kind;
        std::string name;
        std::string file;
        std::string snippet;

        [[nodiscard]] bool operator==(const Requirement&) const = default;
    };

    // The answer to a request: the state the person left, or the requirements the project lacks with the sentence that names them, which fails the request with the code `unsupported`.
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

  private:
    // Returns the text of a key of a JSON object, or an empty text when it holds something else.
    [[nodiscard]] static std::string readText(const core::Json& object, const char* key);

    static const std::array<std::pair<std::string_view, Kind>, 1> kKindNames;
    static const std::array<std::pair<std::string_view, State>, 4> kStateNames;
};

} // namespace haylen::platform
