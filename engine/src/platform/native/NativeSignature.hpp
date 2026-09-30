#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::platform {

// The C declaration of a native callback, such as `void (int code, const char* text, const uint8_t data[size], size_t size)`. Callbacks return nothing, and each parameter is a number, a boolean, a pointer, a text or a range of bytes that an array declarator bounds by a fixed count or by another parameter.
class NativeSignature final {
  public:
    struct Parameter {
        enum class Kind : std::uint8_t {
            Boolean,
            Integer,
            Float,
            Double,
            Pointer,
            Text,
            Bytes,
        };

        Kind kind = Kind::Integer;
        // The size of the C value, or of one element of a range of bytes.
        std::size_t size = 0;
        bool isSigned = false;
        std::string name;
        std::size_t count = 0;
        std::optional<std::size_t> lengthParameter;
    };

    // Throws `std::invalid_argument` that points at what the declaration gets wrong.
    [[nodiscard]] static NativeSignature parse(std::string_view declaration);

    [[nodiscard]] const std::vector<Parameter>& getParameters() const noexcept {
        return parameters;
    }

  private:
    struct BaseType {
        Parameter::Kind kind = Parameter::Kind::Integer;
        std::size_t size = 0;
        bool isSigned = false;
        bool isVoid = false;
        bool isOpaque = false;
        bool isChar = false;
    };

    [[nodiscard]] static std::vector<std::string> tokenize(std::string_view declaration);
    [[nodiscard]] static bool isIdentifier(std::string_view token) noexcept;
    [[nodiscard]] static bool isQualifier(std::string_view token) noexcept;
    [[nodiscard]] static bool isTypeWord(std::string_view token) noexcept;
    [[nodiscard]] static BaseType resolve(std::span<const std::string> words);
    [[nodiscard]] static Parameter parseParameter(std::span<const std::string> tokens, std::size_t& position, std::string& lengthName);
    static void bindLengths(std::vector<Parameter>& parameters, const std::vector<std::string>& lengthNames);

    std::vector<Parameter> parameters;
};

} // namespace haylen::platform
