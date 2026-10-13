#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Connection.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/RingBuffer.hpp"

namespace haylen::debug {

// Runs the commands that the engine, plugins and the app register, from a line of text such as `gold 50` or `say "Hello there"`. Each command declares the kinds of its arguments, which the console checks and converts before the handler runs, and it keeps the history of the lines it ran, the output it printed and completions for a partial line. The console only runs what it is given, so the overlay, the development server and the web page share it.
class Console final {
  public:
    static constexpr std::size_t kHistorySize = 100;
    static constexpr std::size_t kOutputLines = 300;

    // The kind of an argument. A `Text` argument takes the rest of the line as it was typed, so it comes last.
    enum class Kind : std::uint8_t {
        Integer,
        Number,
        String,
        Boolean,
        Text,
    };

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

    struct Line {
        enum class Kind : std::uint8_t {
            Input,
            Output,
            Error,
        };

        Kind kind = Kind::Output;
        std::string text;
    };

    Console();

    // Adds a command, replacing the one with the same name. Disconnecting the connection removes it.
    core::Connection addCommand(Command command);
    bool removeCommand(std::string_view name);
    [[nodiscard]] bool hasCommand(std::string_view name) const;

    // The registered commands in order of name.
    [[nodiscard]] std::vector<const Command*> getCommands() const;

    // Runs one line, records it in the history and prints the line and its output.
    Result run(std::string_view line);

    // Returns the lines that complete a partial line: the names of the commands that start with its only word, and `true` and `false` for a boolean argument.
    [[nodiscard]] std::vector<std::string> complete(std::string_view line) const;

    // The lines that ran, oldest first, without repeating a line that ran twice in a row.
    [[nodiscard]] const core::RingBuffer<std::string>& getHistory() const noexcept {
        return history;
    }
    [[nodiscard]] const core::RingBuffer<Line>& getOutput() const noexcept {
        return output;
    }
    void clearOutput() noexcept {
        output.clear();
    }

    // Prints a line to the output, such as a log line that the console shows.
    void print(Line::Kind kind, std::string_view text);

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
    Result execute(std::string_view line);

    std::vector<std::shared_ptr<Entry>> entries;
    core::RingBuffer<std::string> history{kHistorySize};
    core::RingBuffer<Line> output{kOutputLines};
    core::Connection helpCommand;
};

} // namespace haylen::debug
