#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Connection.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::debug {

// Runs the commands that the engine, plugins and the app register, from a line of text such as `gold 50` or `say "Hello there"`. Each command declares the kinds of its arguments, which the console checks and converts before the handler runs. The console only runs what it is given, so the terminal of the development server, the web page and the app share it.
class Console final {
  public:
    // The kind of an argument. A `Text` argument takes the rest of the line as it was typed, so it comes last.
    enum class Kind : std::uint8_t {
        Integer,
        Number,
        String,
        Boolean,
        Text,
    };

    static constexpr std::array<std::pair<std::string_view, Kind>, 5> kKindNames{{{"integer", Kind::Integer}, {"number", Kind::Number}, {"string", Kind::String}, {"boolean", Kind::Boolean}, {"text", Kind::Text}}};

    struct Parameter {
        Kind kind = Kind::String;
        bool optional = false;
    };

    // Receives the arguments as a JSON array of integers, numbers, strings and booleans, without the optional arguments the line left out, and returns the text to print. Throwing an exception fails the command with its message.
    using Handler = std::function<std::string(const core::Json& arguments)>;

    struct Command {
        std::string name;
        std::string help;
        std::vector<Parameter> parameters;
        Handler handler;
    };

    struct Result {
        bool ok = false;
        std::string output;
    };

    Console();

    // Adds a command, replacing the one with the same name. Disconnecting the connection removes it.
    core::Connection addCommand(Command command);
    bool removeCommand(std::string_view name);
    [[nodiscard]] bool hasCommand(std::string_view name) const;

    // The registered commands in order of name.
    [[nodiscard]] std::vector<const Command*> getCommands() const;

    // Runs one line and returns whether it worked and what it printed.
    Result run(std::string_view line);

    // The name of a kind of argument, such as `integer`.
    [[nodiscard]] static std::string_view kindName(Kind kind) noexcept;

    // Describes how a command is typed, such as `gold <integer> [string]`.
    [[nodiscard]] static std::string describeUsage(const Command& command);

  private:
    struct Token {
        std::string text;
        std::size_t start = 0;
    };

    // A registered command and the link of its connection, which ends once the command is removed or replaced.
    struct Entry final : core::Connection::Link {
        Console* console = nullptr;
        Command command;
        bool blocked = false;

        void disconnect() override;
        [[nodiscard]] bool isConnected() const noexcept override {
            return console != nullptr;
        }
        void setBlocked(bool value) override {
            blocked = value;
        }
        [[nodiscard]] bool isBlocked() const noexcept override {
            return blocked;
        }
    };

    [[nodiscard]] static std::vector<Token> split(std::string_view line);
    [[nodiscard]] static core::Json readArguments(const Command& command, std::string_view line, const std::vector<Token>& tokens);
    [[nodiscard]] static core::Json readArgument(const Command& command, std::size_t index, const std::string& text);
    [[nodiscard]] static std::string_view describeKind(Kind kind) noexcept;
    [[nodiscard]] static std::string_view describePosition(std::size_t index) noexcept;

    [[nodiscard]] std::shared_ptr<Entry> find(std::string_view name) const;
    [[nodiscard]] std::string help(const core::Json& arguments) const;

    std::vector<std::shared_ptr<Entry>> entries;
    core::Connection helpCommand;
};

} // namespace haylen::debug
