#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace haylen::platform {

// The answer to a native dialog. Only the choice of the kind of dialog that was asked for is set, and a dialog the user dismissed or cancelled answers with no choice and no failure.
struct DialogResult {
    // Why a dialog failed: the platform has no such dialog, the app cancelled it, its timeout passed, or the platform could not show it.
    enum class Code : std::uint8_t {
        Unsupported,
        Cancelled,
        Timeout,
        Failed,
    };

    // A file the user picked or saved. Opened files are readable at their path, and a saved file has an empty path where the platform gives none, such as a download of the browser.
    struct File {
        std::string name;
        std::string path;

        [[nodiscard]] bool operator==(const File&) const = default;
    };

    struct Failure {
        Code code = Code::Failed;
        std::string message;
    };

    // The button of a message, counted from zero.
    std::optional<std::size_t> button;

    // The files a picker opened, empty when the user cancelled it.
    std::vector<File> files;

    std::optional<File> saved;
    std::optional<std::string> folder;
    std::optional<Failure> failure;

    [[nodiscard]] static std::string_view codeName(Code value) noexcept;

  private:
    static const std::array<std::pair<std::string_view, Code>, 4> kCodeNames;
};

} // namespace haylen::platform
