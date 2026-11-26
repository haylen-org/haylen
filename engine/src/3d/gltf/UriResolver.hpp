#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "3d/gltf/Diagnostics.hpp"

namespace haylen::gltf {

// Reads the URIs of the buffers and images of a glTF file. A URI is a `data:` URI that holds its bytes, or a relative reference to a file beside the document, with its reserved characters percent-encoded, which must stay inside the content folder of the package. A URI with any other scheme, such as an address on the network, and an absolute path are refused, since a glTF file reads only files of its package.
class UriResolver final {
  public:
    // The parts of a `data:` URI, before its bytes are decoded.
    struct DataUri {
        std::string mediaType;
        bool base64 = false;
        std::string_view payload;
    };

    [[nodiscard]] static bool isDataUri(std::string_view uri) noexcept;

    // Splits a `data:` URI into its media type and its payload. Throws `ReadError` for one without the comma that starts the payload.
    [[nodiscard]] static DataUri parseDataUri(std::string_view uri, std::string_view owner, const Diagnostics& diagnostics);

    // Decodes the payload of a `data:` URI, Base64 or percent-encoded, refusing one whose bytes would pass `maxBytes` before it decodes them.
    [[nodiscard]] static std::vector<std::uint8_t> decodeDataUri(const DataUri& data, std::uint64_t maxBytes, std::string_view owner, const Diagnostics& diagnostics);

    // Resolves a relative reference against the folder of the document into a path of the content folder, such as `textures/wood.png` for `../textures/wood.png` beside `models/ship.gltf`.
    [[nodiscard]] static std::string resolve(std::string_view uri, std::string_view directory, std::string_view owner, const Diagnostics& diagnostics);

  private:
    [[nodiscard]] static std::optional<std::string> percentDecode(std::string_view text);
    [[nodiscard]] static bool hasScheme(std::string_view uri) noexcept;
    [[nodiscard]] static int hexValue(char character) noexcept;
};

} // namespace haylen::gltf
