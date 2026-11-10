#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::conversation {

// A small expression over the variables of a conversation, parsed once: numbers, strings in single or double quotes, `true`, `false`, `nil`, names such as `gold` or `visits.greeting`, the arithmetic `+`, `-`, `*` and `/`, the comparisons `==`, `~=`, `<`, `<=`, `>` and `>=`, and `and`, `or` and `not` with the truth of Lua, where only `false` and `nil` are false.
class Expression final {
  public:
    // Reads the value of a name, or null for a name that has none.
    using Lookup = std::function<core::Json(std::string_view name)>;

    // Throws `std::invalid_argument` with the text and the position of the mistake.
    [[nodiscard]] static Expression parse(std::string_view text);

    [[nodiscard]] core::Json evaluate(const Lookup& lookup) const;
    [[nodiscard]] bool test(const Lookup& lookup) const;

    [[nodiscard]] const std::string& getText() const noexcept {
        return text;
    }

  private:
    enum class Kind : std::uint8_t {
        Literal,
        Name,
        Not,
        Negate,
        Binary,
    };

    struct Term {
        Kind kind = Kind::Literal;
        std::string operation;
        core::Json value;
        std::size_t left = 0;
        std::size_t right = 0;
    };

    // A term that the evaluation reached, and the operands of it that it has evaluated so far.
    struct Visit {
        enum class Stage : std::uint8_t {
            Enter,
            Left,
            Right,
        };

        std::size_t index = 0;
        Stage stage = Stage::Enter;
    };

    // Reads one expression from the text, keeping the position of the next character. It keeps the operators that wait for their operands in lists instead of recursing, so an expression of any depth reads within any native stack.
    class Reader final {
      public:
        Reader(std::string_view source, std::vector<Term>& output) : text(source), terms(output) {}

        [[nodiscard]] std::size_t read();
        [[nodiscard]] bool isDone();

      private:
        // An operator that waits for its right operand, or an open parenthesis, which keeps whether the comparison around it already compared.
        struct Pending {
            Kind kind = Kind::Binary;
            std::string operation;
            int level = 0;
            bool parenthesis = false;
            bool compared = false;
        };

        // The levels at which operators bind, from the loosest to the tightest.
        static constexpr int kOrLevel = 1;
        static constexpr int kAndLevel = 2;
        static constexpr int kNotLevel = 3;
        static constexpr int kComparisonLevel = 4;
        static constexpr int kSumLevel = 5;
        static constexpr int kProductLevel = 6;
        static constexpr int kNegateLevel = 7;

        void readOperand(bool allowsNot);
        [[nodiscard]] std::size_t readValue();
        [[nodiscard]] std::string takeOperation();
        [[nodiscard]] static int getLevel(std::string_view operation) noexcept;
        void reduce(int level);
        void closeParenthesis();
        [[nodiscard]] std::size_t add(Term term);
        [[nodiscard]] bool takeWord(std::string_view word);
        [[nodiscard]] bool takeSymbol(std::string_view symbol);
        [[nodiscard]] std::invalid_argument fail(std::string_view problem) const;
        void skipSpaces() noexcept;

        std::string_view text;
        std::vector<Term>& terms;
        std::vector<Pending> pending;
        std::vector<std::size_t> operands;
        std::size_t position = 0;
        std::size_t parentheses = 0;
        bool compared = false;
    };

    [[nodiscard]] core::Json negate(const core::Json& value) const;
    [[nodiscard]] core::Json combine(const std::string& operation, const core::Json& left, const core::Json& right) const;
    [[nodiscard]] static bool isTrue(const core::Json& value) noexcept;
    [[nodiscard]] double number(const core::Json& value, std::string_view operation) const;

    std::vector<Term> terms;
    std::size_t root = 0;
    std::string text;
};

} // namespace haylen::conversation
