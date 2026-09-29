#include "platform/native/NativeSignature.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace haylen::platform {

NativeSignature NativeSignature::parse(std::string_view declaration) {
    const std::vector<std::string> tokens = tokenize(declaration);
    std::size_t position = 0;

    // Native code may call a callback long before Lua runs it, so it has nothing to return.
    std::vector<std::string> result;
    while (isIdentifier(tokens[position])) {
        if (!isQualifier(tokens[position])) {
            result.push_back(tokens[position]);
        }
        ++position;
    }
    if (result != std::vector<std::string>{"void"} || tokens[position] != "(") {
        throw std::invalid_argument("A native callback returns nothing, so its declaration is void and the parameters, such as void (int code, const char* text).");
    }
    ++position;

    NativeSignature signature;
    std::vector<std::string> lengthNames;
    if (tokens[position] == "void" && tokens[position + 1] == ")") {
        ++position;
    }
    while (tokens[position] != ")") {
        std::string lengthName;
        signature.parameters.push_back(parseParameter(tokens, position, lengthName));
        lengthNames.push_back(std::move(lengthName));
        if (tokens[position].empty()) {
            throw std::invalid_argument("The native callback declaration ends before the ) that closes its parameters.");
        }
        if (tokens[position] != "," && tokens[position] != ")") {
            throw std::invalid_argument("The native callback declaration has '" + tokens[position] + "' where a comma or ) belongs.");
        }
        position += tokens[position] == "," ? 1 : 0;
        if (tokens[position - 1] == "," && tokens[position] == ")") {
            throw std::invalid_argument("The native callback declaration has a comma without a parameter after it.");
        }
    }
    ++position;

    if (!tokens[position].empty()) {
        throw std::invalid_argument("The native callback declaration continues after its parameters with '" + tokens[position] + "'.");
    }
    bindLengths(signature.parameters, lengthNames);
    return signature;
}

// Splits the declaration into names, numbers and punctuation, and ends it with an empty token so the parser never reads past the end.
std::vector<std::string> NativeSignature::tokenize(std::string_view declaration) {
    std::vector<std::string> tokens;
    std::size_t index = 0;
    while (index < declaration.size()) {
        const auto character = static_cast<unsigned char>(declaration[index]);
        if (std::isspace(character) != 0) {
            ++index;
            continue;
        }
        if (std::isalnum(character) != 0 || character == '_') {
            const std::size_t start = index;
            while (index < declaration.size() && (std::isalnum(static_cast<unsigned char>(declaration[index])) != 0 || declaration[index] == '_')) {
                ++index;
            }
            tokens.emplace_back(declaration.substr(start, index - start));
            continue;
        }
        if (std::string_view("()[]*,").find(declaration[index]) == std::string_view::npos) {
            throw std::invalid_argument("The native callback declaration has the unexpected character '" + std::string(1, declaration[index]) + "'.");
        }
        tokens.emplace_back(1, declaration[index]);
        ++index;
    }
    tokens.emplace_back();
    return tokens;
}

bool NativeSignature::isIdentifier(std::string_view token) noexcept {
    return !token.empty() && (std::isalpha(static_cast<unsigned char>(token.front())) != 0 || token.front() == '_');
}

bool NativeSignature::isQualifier(std::string_view token) noexcept {
    return token == "const" || token == "volatile" || token == "restrict";
}

bool NativeSignature::isTypeWord(std::string_view token) noexcept {
    static const std::vector<std::string_view> words{"void", "bool", "_Bool", "char", "signed", "unsigned", "short", "int", "long", "float", "double", "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "size_t", "ssize_t", "ptrdiff_t", "intptr_t", "uintptr_t", "struct", "union", "enum"};
    return std::ranges::find(words, token) != words.end();
}

NativeSignature::BaseType NativeSignature::resolve(std::span<const std::string> words) {
    using Kind = Parameter::Kind;
    // clang-format off
    static const std::unordered_map<std::string, BaseType> types{
        {"void", {.isVoid = true}},
        {"bool", {.kind = Kind::Boolean, .size = sizeof(bool)}},
        {"_Bool", {.kind = Kind::Boolean, .size = sizeof(bool)}},
        {"char", {.size = 1, .isSigned = std::is_signed_v<char>, .isChar = true}},
        {"signed char", {.size = 1, .isSigned = true}},
        {"unsigned char", {.size = 1}},
        {"short", {.size = sizeof(short), .isSigned = true}},
        {"short int", {.size = sizeof(short), .isSigned = true}},
        {"signed short", {.size = sizeof(short), .isSigned = true}},
        {"unsigned short", {.size = sizeof(short)}},
        {"unsigned short int", {.size = sizeof(short)}},
        {"int", {.size = sizeof(int), .isSigned = true}},
        {"signed", {.size = sizeof(int), .isSigned = true}},
        {"signed int", {.size = sizeof(int), .isSigned = true}},
        {"unsigned", {.size = sizeof(int)}},
        {"unsigned int", {.size = sizeof(int)}},
        {"long", {.size = sizeof(long), .isSigned = true}},
        {"long int", {.size = sizeof(long), .isSigned = true}},
        {"signed long", {.size = sizeof(long), .isSigned = true}},
        {"unsigned long", {.size = sizeof(long)}},
        {"unsigned long int", {.size = sizeof(long)}},
        {"long long", {.size = sizeof(long long), .isSigned = true}},
        {"long long int", {.size = sizeof(long long), .isSigned = true}},
        {"signed long long", {.size = sizeof(long long), .isSigned = true}},
        {"unsigned long long", {.size = sizeof(long long)}},
        {"unsigned long long int", {.size = sizeof(long long)}},
        {"float", {.kind = Kind::Float, .size = sizeof(float)}},
        {"double", {.kind = Kind::Double, .size = sizeof(double)}},
        {"int8_t", {.size = 1, .isSigned = true}},
        {"int16_t", {.size = 2, .isSigned = true}},
        {"int32_t", {.size = 4, .isSigned = true}},
        {"int64_t", {.size = 8, .isSigned = true}},
        {"uint8_t", {.size = 1}},
        {"uint16_t", {.size = 2}},
        {"uint32_t", {.size = 4}},
        {"uint64_t", {.size = 8}},
        {"size_t", {.size = sizeof(std::size_t)}},
        {"ssize_t", {.size = sizeof(std::ptrdiff_t), .isSigned = true}},
        {"ptrdiff_t", {.size = sizeof(std::ptrdiff_t), .isSigned = true}},
        {"intptr_t", {.size = sizeof(std::intptr_t), .isSigned = true}},
        {"uintptr_t", {.size = sizeof(std::uintptr_t)}},
    };
    // clang-format on

    if (words.empty()) {
        throw std::invalid_argument("A parameter of the native callback declaration has no type.");
    }
    if (words.size() == 2 && (words[0] == "struct" || words[0] == "union")) {
        return {.isOpaque = true};
    }
    if (words.size() == 2 && words[0] == "enum") {
        return {.size = sizeof(int), .isSigned = true};
    }

    std::string name = words[0];
    for (std::size_t index = 1; index < words.size(); ++index) {
        name += " " + words[index];
    }
    if (const auto found = types.find(name); found != types.end()) {
        return found->second;
    }
    if (words.size() == 1 && !isTypeWord(words[0])) {
        return {.isOpaque = true};
    }
    throw std::invalid_argument("The native callback declaration has the unknown type '" + name + "'.");
}

NativeSignature::Parameter NativeSignature::parseParameter(std::span<const std::string> tokens, std::size_t& position, std::string& lengthName) {
    std::vector<std::string> words;
    bool constant = false;
    while (isIdentifier(tokens[position])) {
        if (isQualifier(tokens[position])) {
            constant = constant || tokens[position] == "const";
        } else {
            words.push_back(tokens[position]);
        }
        ++position;
    }

    int pointers = 0;
    while (tokens[position] == "*" || isQualifier(tokens[position])) {
        pointers += tokens[position] == "*" ? 1 : 0;
        ++position;
    }

    // A name follows the stars, or it is the last word when it cannot be part of the type.
    Parameter parameter;
    if (isIdentifier(tokens[position])) {
        parameter.name = tokens[position++];
    } else if (pointers == 0 && words.size() > 1 && !isTypeWord(words.back())) {
        const std::string& previous = words[words.size() - 2];
        if (previous != "struct" && previous != "union" && previous != "enum") {
            parameter.name = words.back();
            words.pop_back();
        }
    }
    const BaseType base = resolve(words);
    const std::string label = parameter.name.empty() ? "A parameter" : "The parameter " + parameter.name;

    if (tokens[position] == "[") {
        const std::string& bound = tokens[position + 1];
        if (tokens[position + 2] != "]" || bound.empty() || !(std::isdigit(static_cast<unsigned char>(bound.front())) != 0 || isIdentifier(bound))) {
            throw std::invalid_argument(label + " of the native callback declaration needs a count or the name of a length parameter between [ and ].");
        }
        position += 3;
        if (pointers == 0 && (base.isVoid || base.isOpaque)) {
            throw std::invalid_argument(label + " is a range of elements whose size the native callback declaration does not know. Use uint8_t or another C number type.");
        }
        parameter.kind = Parameter::Kind::Bytes;
        parameter.size = pointers > 0 ? sizeof(void*) : base.size;
        if (isIdentifier(bound)) {
            lengthName = bound;
        } else {
            parameter.count = std::stoul(bound);
        }
        return parameter;
    }

    if (pointers > 0) {
        parameter.kind = pointers == 1 && base.isChar && constant ? Parameter::Kind::Text : Parameter::Kind::Pointer;
        parameter.size = sizeof(void*);
        return parameter;
    }
    if (base.isVoid || base.isOpaque) {
        throw std::invalid_argument(label + " of the native callback declaration is not a C number type, and a callback reads other values only through pointers.");
    }
    parameter.kind = base.kind;
    parameter.size = base.size;
    parameter.isSigned = base.isSigned;
    return parameter;
}

// A range of bytes names the integer parameter that holds its element count.
void NativeSignature::bindLengths(std::vector<Parameter>& parameters, const std::vector<std::string>& lengthNames) {
    for (std::size_t index = 0; index < parameters.size(); ++index) {
        if (lengthNames[index].empty()) {
            continue;
        }
        const auto found = std::ranges::find(parameters, lengthNames[index], &Parameter::name);
        if (found == parameters.end() || found->kind != Parameter::Kind::Integer) {
            throw std::invalid_argument("The length " + lengthNames[index] + " of the native callback parameter " + parameters[index].name + " must name an integer parameter.");
        }
        parameters[index].lengthParameter = static_cast<std::size_t>(found - parameters.begin());
    }
}

} // namespace haylen::platform
