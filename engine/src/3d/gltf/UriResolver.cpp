#include "3d/gltf/UriResolver.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "core/Base64.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::gltf {

bool UriResolver::isDataUri(std::string_view uri) noexcept {
    constexpr std::string_view kScheme = "data:";
    if (uri.size() < kScheme.size()) {
        return false;
    }

    // clang-format off
    return std::ranges::equal(uri.substr(0, kScheme.size()), kScheme, [](char lhs, char rhs) {
        return std::tolower(static_cast<unsigned char>(lhs)) == rhs;
    });
    // clang-format on
}

UriResolver::DataUri UriResolver::parseDataUri(std::string_view uri, std::string_view owner, const Diagnostics& diagnostics) {
    const std::string_view rest = uri.substr(5);
    const std::size_t comma = rest.find(',');
    if (comma == std::string_view::npos) {
        diagnostics.invalid(ReadError::Problem::InvalidUri, "the \"data:\" URI of " + std::string(owner) + " has no comma before its data");
    }

    std::string header(rest.substr(0, comma));
    std::ranges::transform(header, header.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

    DataUri data;
    data.payload = rest.substr(comma + 1);
    constexpr std::string_view kBase64 = ";base64";
    if (header.ends_with(kBase64)) {
        data.base64 = true;
        header.resize(header.size() - kBase64.size());
    }

    data.mediaType = header.substr(0, header.find(';'));
    return data;
}

std::vector<std::uint8_t> UriResolver::decodeDataUri(const DataUri& data, std::uint64_t maxBytes, std::string_view owner, const Diagnostics& diagnostics) {
    const std::uint64_t largest = data.base64 ? data.payload.size() / 4 * 3 + 3 : data.payload.size();
    if (largest > maxBytes) {
        diagnostics.refuse(ReadError::Problem::LimitExceeded, "holds a \"data:\" URI in " + std::string(owner) + " of more than the " + std::to_string(maxBytes) + " bytes the reader takes from one", "Keep large buffers and images in a GLB file or in files beside the model.");
    }

    std::optional<std::vector<std::uint8_t>> bytes;
    if (data.base64) {
        bytes = core::Base64::decode(data.payload);
    } else if (const std::optional<std::string> text = percentDecode(data.payload)) {
        bytes.emplace(text->begin(), text->end());
    }

    if (!bytes) {
        diagnostics.invalid(ReadError::Problem::InvalidUri, "the data of the \"data:\" URI of " + std::string(owner) + " is not valid " + std::string(data.base64 ? "Base64" : "percent-encoded text"));
    }

    return std::move(*bytes);
}

std::string UriResolver::resolve(std::string_view uri, std::string_view directory, std::string_view owner, const Diagnostics& diagnostics) {
    using Problem = ReadError::Problem;

    const std::string named = "the URI \"" + std::string(uri) + "\" of " + std::string(owner);
    constexpr std::string_view kPackageOnly = "A glTF file reads only files inside its package, which it names by paths relative to itself, such as \"textures/wood.png\".";
    if (hasScheme(uri)) {
        diagnostics.refuse(Problem::UriScheme, "names " + named + ", which is an address and not a file of the package", kPackageOnly);
    }

    if (uri.empty() || uri.find_first_of("?#\\") != std::string_view::npos) {
        diagnostics.invalid(Problem::InvalidUri, named + " is not a relative reference to a file, which holds no query, fragment or backslash");
    }

    const std::optional<std::string> decoded = percentDecode(uri);
    if (!decoded || decoded->empty() || decoded->find('\0') != std::string::npos) {
        diagnostics.invalid(Problem::InvalidUri, named + " holds a percent sign that does not start a byte of two hexadecimal digits");
    }

    if (decoded->starts_with('/')) {
        diagnostics.refuse(Problem::FileOutsidePackage, "names " + named + ", which is an absolute path", kPackageOnly);
    }

    try {
        return io::Path::normalize(io::Path::join(directory, *decoded));
    } catch (const std::invalid_argument&) {
        diagnostics.refuse(Problem::FileOutsidePackage, "names " + named + ", which leaves the content folder of the package", kPackageOnly);
    }
}

std::optional<std::string> UriResolver::percentDecode(std::string_view text) {
    std::string decoded;
    decoded.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] != '%') {
            decoded.push_back(text[index]);
            continue;
        }

        if (index + 2 >= text.size()) {
            return std::nullopt;
        }

        const int high = hexValue(text[index + 1]);
        const int low = hexValue(text[index + 2]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }

        decoded.push_back(static_cast<char>(high * 16 + low));
        index += 2;
    }

    return decoded;
}

bool UriResolver::hasScheme(std::string_view uri) noexcept {
    const std::size_t colon = uri.find(':');
    if (colon == std::string_view::npos || colon == 0 || uri.find_first_of("/?#") < colon) {
        return false;
    }

    if (std::isalpha(static_cast<unsigned char>(uri[0])) == 0) {
        return false;
    }

    // clang-format off
    return std::ranges::all_of(uri.substr(1, colon - 1), [](unsigned char character) {
        return std::isalnum(character) != 0 || character == '+' || character == '-' || character == '.';
    });
    // clang-format on
}

int UriResolver::hexValue(char character) noexcept {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }

    const int lower = std::tolower(static_cast<unsigned char>(character));
    if (lower >= 'a' && lower <= 'f') {
        return lower - 'a' + 10;
    }

    return -1;
}

} // namespace haylen::gltf
