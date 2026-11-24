#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::core {

// An app error split into the parts an error screen and an editor show: the message, the position in the code of the app where it rose and the stack of calls that led to it, innermost first. The method `what()` returns the text as it was raised, and errors raised outside the code of the app keep an empty file, line zero and no frames.
class AppError final : public std::runtime_error {
  public:
    // What kind of failure the error is, which an editor tells apart without reading the message: code that raised an error as it ran, a source file that does not compile, a package that cannot open or whose `haylen.json` is invalid, an asset that failed to load, code that ran longer than the execution budget of development, or an app that ran out of memory, of its memory budget in development or of the system.
    enum class Code : std::uint8_t {
        Runtime,
        Syntax,
        Package,
        Asset,
        ExecutionBudget,
        Memory,
    };
    static constexpr std::array<std::string_view, 6> kCodeNames = {"runtime", "syntax", "package", "asset", "executionBudget", "memory"};

    struct Frame {
        // Whether the frame runs code of a script, a native function or the main chunk of a script.
        enum class Kind : std::uint8_t {
            Script,
            Native,
            Main,
        };
        static constexpr std::array<std::string_view, 3> kKindNames = {"script", "native", "main"};

        // The chunk as the script runtime shows it, such as `source/main.lua`, or `[C]` for native functions.
        std::string source;
        int line = 0;
        // The function as the script runtime describes it, such as `method 'update'`, `local 'spawn'` or `main chunk`.
        std::string function;
        Kind kind = Kind::Script;

        // Returns `source:line`, or the source alone when the frame has no current line.
        [[nodiscard]] std::string getLocation() const;
    };

    // The parts of an error that the runtime of the code that raised it read: the message without its position, the file and line of that position and the stack.
    struct Details {
        std::string message;
        std::string file{};
        int line = 0;
        std::vector<Frame> frames{};
    };

    // An error whose message is its text, with no position and no frames.
    explicit AppError(const std::string& text);
    AppError(const std::string& text, Details details);

    [[nodiscard]] const std::string& getMessage() const noexcept {
        return message;
    }
    [[nodiscard]] const std::string& getFile() const noexcept {
        return file;
    }
    [[nodiscard]] int getLine() const noexcept {
        return line;
    }
    [[nodiscard]] const std::vector<Frame>& getFrames() const noexcept {
        return frames;
    }
    [[nodiscard]] Code getCode() const noexcept {
        return code;
    }
    void setCode(Code value) noexcept {
        code = value;
    }

    // The step of the engine that ran the failing code, such as `update` or `reload`, where the engine tells it, or an empty text.
    [[nodiscard]] const std::string& getCallback() const noexcept {
        return callback;
    }
    void setCallback(std::string value) {
        callback = std::move(value);
    }

    // Renders the frames one per line as aligned location and function columns, separated by spaces and never by tab characters.
    [[nodiscard]] std::string getTraceback() const;
    [[nodiscard]] Json toJson() const;

  private:
    std::string message;
    std::string file;
    int line = 0;
    std::vector<Frame> frames;
    Code code = Code::Runtime;
    std::string callback;
};

} // namespace haylen::core
