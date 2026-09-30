#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace haylen::platform {

// A native dialog the app asks for: a message with buttons, or a picker of files to open, of the destination of data to save or of a folder. The class `Dialogs` validates it before the platform shows it.
struct DialogRequest {
    enum class MessageKind : std::uint8_t {
        Info,
        Warning,
        Error,
    };

    // A named choice of file types, such as Images with `png` and `jpg`. Extensions come without their dot.
    struct Filter {
        std::string name;
        std::vector<std::string> extensions;
    };

    // A message with one to three buttons, whose labels the app gives in its own language.
    struct Message {
        std::string title;
        std::string text;
        MessageKind kind = MessageKind::Info;
        std::vector<std::string> buttons;
    };

    struct OpenFiles {
        std::string title;
        std::vector<Filter> filters;
        bool multiple = false;
    };

    // The platform suggests the name for the file and writes the data to the destination the user picks.
    struct SaveFile {
        std::string title;
        std::vector<Filter> filters;
        std::string name;
        std::vector<std::uint8_t> data;
    };

    struct OpenFolder {
        std::string title;
    };

    std::variant<Message, OpenFiles, SaveFile, OpenFolder> dialog;

    [[nodiscard]] static std::string_view messageKindName(MessageKind value) noexcept;
    [[nodiscard]] static std::optional<MessageKind> messageKindFromName(std::string_view name) noexcept;

  private:
    static const std::array<std::pair<std::string_view, MessageKind>, 3> kMessageKindNames;
};

} // namespace haylen::platform
