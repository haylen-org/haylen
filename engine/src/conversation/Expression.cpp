#include "haylen/conversation/Expression.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <format>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <fast_float/fast_float.h>

namespace haylen::conversation {

Expression Expression::parse(std::string_view text) {
    Expression expression;
    expression.text = std::string(text);
    Reader reader(expression.text, expression.terms);
    expression.root = reader.read();
    if (!reader.isDone()) {
        throw std::invalid_argument(std::format("The expression \"{}\" has text after its end.", text));
    }
    return expression;
}

bool Expression::test(const Lookup& lookup) const {
    return isTrue(evaluate(lookup));
}

bool Expression::isTrue(const core::Json& value) noexcept {
    return !value.is_null() && !(value.is_boolean() && !value.get<bool>());
}

double Expression::number(const core::Json& value, std::string_view operation) const {
    if (!value.is_number()) {
        throw std::invalid_argument(std::format("The expression \"{}\" uses \"{}\" with a value that is not a number.", text, operation));
    }
    return value.get<double>();
}

// Each term is visited from a list instead of a recursion, so an expression of any depth evaluates within any native stack. The value of every evaluated operand waits in a list until its operator takes it, and `and` and `or` evaluate their right operand only when the left one does not settle them.
core::Json Expression::evaluate(const Lookup& lookup) const {
    std::vector<Visit> visits{{.index = root, .stage = Visit::Stage::Enter}};
    std::vector<core::Json> values;
    while (!visits.empty()) {
        const Visit visit = visits.back();
        const Term& term = terms[visit.index];

        if (term.kind == Kind::Literal || term.kind == Kind::Name) {
            visits.pop_back();
            values.push_back(term.kind == Kind::Literal ? term.value : lookup(term.value.get_ref<const std::string&>()));
            continue;
        }

        if (visit.stage == Visit::Stage::Enter) {
            visits.back().stage = Visit::Stage::Left;
            visits.push_back({.index = term.left, .stage = Visit::Stage::Enter});
            continue;
        }

        if (term.kind == Kind::Not || term.kind == Kind::Negate) {
            visits.pop_back();
            values.back() = term.kind == Kind::Not ? core::Json(!isTrue(values.back())) : negate(values.back());
            continue;
        }

        const bool logical = term.operation == "and" || term.operation == "or";
        if (visit.stage == Visit::Stage::Left && logical) {
            const bool settled = term.operation == "and" ? !isTrue(values.back()) : isTrue(values.back());
            if (settled) {
                visits.pop_back();
                continue;
            }
            values.pop_back();
            visits.back() = {.index = term.right, .stage = Visit::Stage::Enter};
            continue;
        }

        if (visit.stage == Visit::Stage::Left) {
            visits.back().stage = Visit::Stage::Right;
            visits.push_back({.index = term.right, .stage = Visit::Stage::Enter});
            continue;
        }

        visits.pop_back();
        const core::Json right = std::move(values.back());
        values.pop_back();
        values.back() = combine(term.operation, values.back(), right);
    }
    return std::move(values.back());
}

core::Json Expression::negate(const core::Json& value) const {
    if (value.is_number_integer()) {
        return static_cast<std::int64_t>(0U - static_cast<std::uint64_t>(value.get<std::int64_t>()));
    }
    return -number(value, "-");
}

// Integers stay integers through addition, subtraction and multiplication, so counters keep their type, and they wrap around past the range of 64 bits, as in Lua.
core::Json Expression::combine(const std::string& operation, const core::Json& left, const core::Json& right) const {
    if (operation == "==") {
        return left.is_number() && right.is_number() ? left.get<double>() == right.get<double>() : left == right;
    }
    if (operation == "~=") {
        return left.is_number() && right.is_number() ? left.get<double>() != right.get<double>() : left != right;
    }
    if ((operation == "+" || operation == "-" || operation == "*") && left.is_number_integer() && right.is_number_integer()) {
        const auto a = static_cast<std::uint64_t>(left.get<std::int64_t>());
        const auto b = static_cast<std::uint64_t>(right.get<std::int64_t>());
        return static_cast<std::int64_t>(operation == "+" ? a + b : operation == "-" ? a - b : a * b);
    }
    const double a = number(left, operation);
    const double b = number(right, operation);
    if (operation == "+") {
        return a + b;
    }
    if (operation == "-") {
        return a - b;
    }
    if (operation == "*") {
        return a * b;
    }
    if (operation == "/") {
        return a / b;
    }
    if (operation == "<") {
        return a < b;
    }
    if (operation == "<=") {
        return a <= b;
    }
    if (operation == ">") {
        return a > b;
    }
    return a >= b;
}

bool Expression::Reader::isDone() {
    skipSpaces();
    return position == text.size();
}

// Operators wait in a list until an operator that binds as loosely or more loosely arrives, and a closing parenthesis applies those of its level. A comparison takes no second comparison beside it, and `not` stands only where a whole condition begins: at the start, after `(`, `and`, `or` and another `not`.
std::size_t Expression::Reader::read() {
    readOperand(true);
    while (true) {
        const std::string operation = takeOperation();
        if (!operation.empty()) {
            const int level = getLevel(operation);
            reduce(level);
            pending.push_back({.kind = Kind::Binary, .operation = operation, .level = level, .parenthesis = false, .compared = false});
            const bool logical = level == kOrLevel || level == kAndLevel;
            compared = !logical && (compared || level == kComparisonLevel);
            readOperand(logical);
            continue;
        }

        if (parentheses == 0) {
            break;
        }
        if (!takeSymbol(")")) {
            throw fail("misses a closing parenthesis");
        }
        closeParenthesis();
    }

    reduce(0);
    return operands.back();
}

void Expression::Reader::readOperand(bool allowsNot) {
    while (true) {
        if (allowsNot && takeWord("not")) {
            pending.push_back({.kind = Kind::Not, .operation = {}, .level = kNotLevel, .parenthesis = false, .compared = false});
            continue;
        }
        if (takeSymbol("-")) {
            pending.push_back({.kind = Kind::Negate, .operation = {}, .level = kNegateLevel, .parenthesis = false, .compared = false});
            allowsNot = false;
            continue;
        }

        skipSpaces();
        if (position == text.size()) {
            throw fail("ends where a value belongs");
        }
        if (takeSymbol("(")) {
            pending.push_back({.kind = Kind::Binary, .operation = {}, .level = 0, .parenthesis = true, .compared = compared});
            ++parentheses;
            compared = false;
            allowsNot = true;
            continue;
        }

        operands.push_back(readValue());
        return;
    }
}

std::string Expression::Reader::takeOperation() {
    for (const std::string_view operation : {"*", "/", "+", "-"}) {
        if (takeSymbol(operation)) {
            return std::string(operation);
        }
    }
    if (!compared) {
        for (const std::string_view operation : {"==", "~=", "!=", "<=", ">=", "<", ">"}) {
            if (takeSymbol(operation)) {
                return operation == "!=" ? "~=" : std::string(operation);
            }
        }
    }
    for (const std::string_view operation : {"and", "or"}) {
        if (takeWord(operation)) {
            return std::string(operation);
        }
    }
    return {};
}

int Expression::Reader::getLevel(std::string_view operation) noexcept {
    if (operation == "or") {
        return kOrLevel;
    }
    if (operation == "and") {
        return kAndLevel;
    }
    if (operation == "+" || operation == "-") {
        return kSumLevel;
    }
    if (operation == "*" || operation == "/") {
        return kProductLevel;
    }
    return kComparisonLevel;
}

// Applies the waiting operators that bind at least as tightly as the level, down to the innermost open parenthesis, to the operands they wait for.
void Expression::Reader::reduce(int level) {
    while (!pending.empty() && !pending.back().parenthesis && pending.back().level >= level) {
        Pending applied = std::move(pending.back());
        pending.pop_back();

        if (applied.kind != Kind::Binary) {
            operands.back() = add({.kind = applied.kind, .operation = {}, .value = {}, .left = operands.back(), .right = 0});
            continue;
        }

        const std::size_t right = operands.back();
        operands.pop_back();
        operands.back() = add({.kind = Kind::Binary, .operation = std::move(applied.operation), .value = {}, .left = operands.back(), .right = right});
    }
}

void Expression::Reader::closeParenthesis() {
    reduce(0);
    compared = pending.back().compared;
    pending.pop_back();
    --parentheses;
}

std::size_t Expression::Reader::readValue() {
    const char first = text[position];
    if (first == '"' || first == '\'') {
        const std::size_t end = text.find(first, position + 1);
        if (end == std::string_view::npos) {
            throw fail("has a string that is not closed");
        }
        std::string value(text.substr(position + 1, end - position - 1));
        position = end + 1;
        return add({.kind = Kind::Literal, .operation = {}, .value = std::move(value), .left = 0, .right = 0});
    }
    if (std::isdigit(static_cast<unsigned char>(first)) != 0) {
        double value = 0.0;
        const auto [end, error] = fast_float::from_chars(text.data() + position, text.data() + text.size(), value);
        if (error != std::errc{}) {
            throw fail("has a number it cannot read");
        }
        const std::string_view digits = text.substr(position, static_cast<std::size_t>(end - (text.data() + position)));
        position += digits.size();

        // Whole numbers that fit 64 bits are integers with every digit, and longer ones stay floats, as in Lua.
        std::int64_t whole = 0;
        const bool integral = digits.find_first_of(".eE") == std::string_view::npos && std::from_chars(digits.data(), digits.data() + digits.size(), whole).ec == std::errc{};
        return add({.kind = Kind::Literal, .operation = {}, .value = integral ? core::Json(whole) : core::Json(value), .left = 0, .right = 0});
    }
    if (std::isalpha(static_cast<unsigned char>(first)) != 0 || first == '_') {
        const std::size_t start = position;
        while (position < text.size() && (std::isalnum(static_cast<unsigned char>(text[position])) != 0 || text[position] == '_' || text[position] == '.')) {
            ++position;
        }
        const std::string_view name = text.substr(start, position - start);
        if (name == "true" || name == "false") {
            return add({.kind = Kind::Literal, .operation = {}, .value = name == "true", .left = 0, .right = 0});
        }
        if (name == "nil") {
            return add({.kind = Kind::Literal, .operation = {}, .value = nullptr, .left = 0, .right = 0});
        }
        if (name == "and" || name == "or" || name == "not") {
            position = start;
            throw fail(std::format("has \"{}\" where a value belongs", name));
        }
        return add({.kind = Kind::Name, .operation = {}, .value = std::string(name), .left = 0, .right = 0});
    }
    throw fail(std::format("has \"{}\" where a value belongs", first));
}

std::size_t Expression::Reader::add(Term term) {
    terms.push_back(std::move(term));
    return terms.size() - 1;
}

// A word only counts as a whole word, so `order` never starts with `or`.
bool Expression::Reader::takeWord(std::string_view word) {
    skipSpaces();
    if (!text.substr(position).starts_with(word)) {
        return false;
    }
    const std::size_t end = position + word.size();
    if (end < text.size() && (std::isalnum(static_cast<unsigned char>(text[end])) != 0 || text[end] == '_')) {
        return false;
    }
    position = end;
    return true;
}

bool Expression::Reader::takeSymbol(std::string_view symbol) {
    skipSpaces();
    if (!text.substr(position).starts_with(symbol)) {
        return false;
    }
    position += symbol.size();
    return true;
}

std::invalid_argument Expression::Reader::fail(std::string_view problem) const {
    return std::invalid_argument(std::format("The expression \"{}\" {} at character {}.", text, problem, position + 1));
}

void Expression::Reader::skipSpaces() noexcept {
    while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])) != 0) {
        ++position;
    }
}

} // namespace haylen::conversation
