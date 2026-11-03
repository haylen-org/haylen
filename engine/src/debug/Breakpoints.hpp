#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace haylen::debug {

// The breakpoints of the Lua debugger, indexed by the package path of their file and their line, so the hooks find out at once whether a function or a line holds one. A breakpoint stops only when its condition holds and its count of hits passes, and a log point writes its message instead of stopping.
class Breakpoints final {
  public:
    // When a breakpoint stops by the count of its hits: always, at the hit of the count, from that hit on, after it or at every multiple of it.
    struct HitCondition {
        enum class Operator : std::uint8_t {
            Always,
            Equal,
            AtLeast,
            Greater,
            Multiple,
        };

        Operator op = Operator::Always;
        int count = 0;

        // Reads the count alone, which stops at that hit, or the count after `==`, `>=`, `>` or `%`. Returns nothing for any other text.
        [[nodiscard]] static std::optional<HitCondition> parse(std::string_view text);
        [[nodiscard]] bool passes(int hits) const noexcept;
    };

    // What the client asks for at a line.
    struct Request {
        int line = 0;
        std::string condition;
        std::string hitCondition;
        std::string logMessage;
    };

    struct Breakpoint {
        int id = 0;
        int line = 0;
        std::string condition;
        HitCondition hit;
        std::string logMessage;
        int hits = 0;
    };

    // What the client hears of a breakpoint it asked for: the id and the line, and whether it takes effect, with the reason when it does not.
    struct Result {
        int id = 0;
        int line = 0;
        bool verified = false;
        std::string message;
    };

    // The breakpoints of one file by their line.
    class File final {
      public:
        // Whether a line from the first to the last holds a breakpoint.
        [[nodiscard]] bool covers(int first, int last) const;

        [[nodiscard]] Breakpoint* find(int line);
        [[nodiscard]] bool empty() const noexcept {
            return lines.empty();
        }

      private:
        friend class Breakpoints;

        std::map<int, Breakpoint> lines;
    };

    // Replaces every breakpoint of the file at the package path, such as `source/main.lua`, and returns what each request became, in order.
    [[nodiscard]] std::vector<Result> set(const std::string& path, const std::vector<Request>& requests);
    void clear();

    // Finds the breakpoints of the file that a chunk named `@path` came from.
    [[nodiscard]] File* find(std::string_view source);

    [[nodiscard]] bool empty() const noexcept {
        return files.empty();
    }

    // Whether any file holds a breakpoint at the line, which the hook of a line asks before it reads where it runs.
    [[nodiscard]] bool hasLine(int line) const noexcept {
        return line >= 0 && static_cast<std::size_t>(line) < lineCounts.size() && lineCounts[static_cast<std::size_t>(line)] > 0;
    }

  private:
    struct Hash {
        using is_transparent = void;
        [[nodiscard]] std::size_t operator()(std::string_view value) const noexcept {
            return std::hash<std::string_view>{}(value);
        }
    };

    void count(const File& file, int delta);

    std::unordered_map<std::string, File, Hash, std::equal_to<>> files;
    std::vector<int> lineCounts;
    int nextId = 1;
};

} // namespace haylen::debug
