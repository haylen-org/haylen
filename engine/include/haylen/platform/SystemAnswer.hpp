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

// The answer of a call that the system may refuse, such as fullscreen, a mode of the mouse, a screen kept on, the clipboard or the persistence of the storage of a browser: `Granted` once the call did its work, `Prompt` where the system did not show the prompt that the call needs, such as a browser without a recent input of the person, `Denied` when the person or the system refused it, and `Unsupported` where the platform has no such capability or the project of the app lacks a requirement, with the sentence that says why and what to do.
struct SystemAnswer {
    enum class State : std::uint8_t {
        Granted,
        Prompt,
        Denied,
        Unsupported,
    };

    // What the app lacks for the call, such as a feature of the permissions policy of a page, with the file that adds it and the snippet that the file takes.
    struct Requirement {
        std::string kind;
        std::string name;
        std::string file;
        std::string snippet;

        [[nodiscard]] bool operator==(const Requirement&) const = default;
    };

    State state = State::Unsupported;
    std::vector<Requirement> missing{};
    std::string message{};

    // Reads the JSON object `{state, missing, message}` that native code answers with, where `missing` lists `{kind, name, file, snippet}`.
    [[nodiscard]] static SystemAnswer fromJson(const core::Json& value);
    [[nodiscard]] core::Json missingToJson() const;

    [[nodiscard]] static std::string_view stateName(State value) noexcept;
    [[nodiscard]] static std::optional<State> stateFromName(std::string_view name) noexcept;

  private:
    // Returns the text of a key of a JSON object, or an empty text when it holds something else.
    [[nodiscard]] static std::string readText(const core::Json& object, const char* key);

    static const std::array<std::pair<std::string_view, State>, 4> kStateNames;
};

} // namespace haylen::platform
