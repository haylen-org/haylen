#include "haylen/conversation/Expression.hpp"

#include <cctype>
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
    expression.root = reader.readOr();
    if (!reader.isDone()) {
        throw std::invalid_argument(std::format("The expression \"{}\" has text after its end.", text));
    }
    return expression;
}

core::Json Expression::evaluate(const Lookup& lookup) const {
    return evaluate(root, lookup);
}

bool Expression::test(const Lookup& lookup) const {
    return isTrue(evaluate(root, lookup));
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

// Integers stay integers through addition, subtraction and multiplication, so counters keep their type.
core::Json Expression::evaluate(std::size_t index, const Lookup& lookup) const {
    const Term& term = terms[index];
    switch (term.kind) {
    case Kind::Literal:
        return term.value;
    case Kind::Name:
        return lookup(term.value.get_ref<const std::string&>());
    case Kind::Not:
        return !isTrue(evaluate(term.left, lookup));
    case Kind::Negate: {
        const core::Json value = evaluate(term.left, lookup);
        return value.is_number_integer() ? core::Json(-value.get<std::int64_t>()) : core::Json(-number(value, "-"));
    }
    case Kind::Binary:
        break;
    }

    const std::string& operation = term.operation;
    const core::Json left = evaluate(term.left, lookup);
    if (operation == "and") {
        return isTrue(left) ? evaluate(term.right, lookup) : left;
    }
    if (operation == "or") {
        return isTrue(left) ? left : evaluate(term.right, lookup);
    }
    const core::Json right = evaluate(term.right, lookup);
    if (operation == "==") {
        return left.is_number() && right.is_number() ? left.get<double>() == right.get<double>() : left == right;
    }
    if (operation == "~=") {
        return left.is_number() && right.is_number() ? left.get<double>() != right.get<double>() : left != right;
    }
    if ((operation == "+" || operation == "-" || operation == "*") && left.is_number_integer() && right.is_number_integer()) {
        const std::int64_t a = left.get<std::int64_t>();
        const std::int64_t b = right.get<std::int64_t>();
        return operation == "+" ? a + b : operation == "-" ? a - b : a * b;
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

std::size_t Expression::Reader::readOr() {
    std::size_t left = readAnd();
    while (takeWord("or")) {
        left = binary("or", left, readAnd());
    }
    return left;
}

std::size_t Expression::Reader::readAnd() {
    std::size_t left = readNot();
    while (takeWord("and")) {
        left = binary("and", left, readNot());
    }
    return left;
}

std::size_t Expression::Reader::readNot() {
    if (takeWord("not")) {
        return add({.kind = Kind::Not, .operation = {}, .value = {}, .left = readNot(), .right = 0});
    }
    return readComparison();
}

std::size_t Expression::Reader::readComparison() {
    const std::size_t left = readSum();
    for (const std::string_view operation : {"==", "~=", "!=", "<=", ">=", "<", ">"}) {
        if (takeSymbol(operation)) {
            return binary(operation == "!=" ? "~=" : std::string(operation), left, readSum());
        }
    }
    return left;
}

std::size_t Expression::Reader::readSum() {
    std::size_t left = readProduct();
    while (true) {
        if (takeSymbol("+")) {
            left = binary("+", left, readProduct());
        } else if (takeSymbol("-")) {
            left = binary("-", left, readProduct());
        } else {
            return left;
        }
    }
}

std::size_t Expression::Reader::readProduct() {
    std::size_t left = readUnary();
    while (true) {
        if (takeSymbol("*")) {
            left = binary("*", left, readUnary());
        } else if (takeSymbol("/")) {
            left = binary("/", left, readUnary());
        } else {
            return left;
        }
    }
}

std::size_t Expression::Reader::readUnary() {
    if (takeSymbol("-")) {
        return add({.kind = Kind::Negate, .operation = {}, .value = {}, .left = readUnary(), .right = 0});
    }
    return readPrimary();
}

std::size_t Expression::Reader::readPrimary() {
    skipSpaces();
    if (position == text.size()) {
        throw fail("ends where a value belongs");
    }
    if (takeSymbol("(")) {
        const std::size_t inner = readOr();
        if (!takeSymbol(")")) {
            throw fail("misses a closing parenthesis");
        }
        return inner;
    }

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
        const bool whole = digits.find_first_of(".eE") == std::string_view::npos;
        return add({.kind = Kind::Literal, .operation = {}, .value = whole ? core::Json(static_cast<std::int64_t>(value)) : core::Json(value), .left = 0, .right = 0});
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

std::size_t Expression::Reader::binary(std::string operation, std::size_t left, std::size_t right) {
    return add({.kind = Kind::Binary, .operation = std::move(operation), .value = {}, .left = left, .right = right});
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
