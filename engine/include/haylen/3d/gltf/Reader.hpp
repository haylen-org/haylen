#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/3d/gltf/Document.hpp"
#include "haylen/3d/gltf/Limits.hpp"
#include "haylen/3d/gltf/ReadError.hpp"

namespace haylen::io {
class Package;
class PackageReader;
} // namespace haylen::io

namespace haylen::gltf {

// Reads a glTF 2.0 file, a GLB file or a `.gltf` text file, into a document of CPU-side data, treating every byte as hostile. A file in a package reads its buffers and images from the binary chunk of the GLB file, from files beside it inside the package or from `data:` URIs, and bytes in memory read only a GLB file or a `.gltf` file whose resources are `data:` URIs. Reading runs in three steps, so a caller can bring the files beside a document onto the device between them: `open` reads the document and checks its shape and links, `loadResources` reads its buffers and images, and `finish` checks the data of its accessors and hands the document out. Every step throws `ReadError` for a file that breaks a rule of glTF 2.0 or a limit. Opening and loading read files and belong on the I/O pool, and finishing is one pass over the data that belongs on the task pool.
class Reader final {
  public:
    // The extensions the reader knows besides the core of glTF 2.0. A file that requires any other extension fails with a sentence that names it, and other extensions that a file only uses are kept as their JSON.
    static constexpr std::array<std::string_view, 1> kSupportedExtensions{"EXT_texture_webp"};

    [[nodiscard]] static Reader open(const io::Package& package, std::string_view path, Limits limits = {});

    // Opens bytes in memory, which stay alive and unchanged until `loadResources` returns, so the binary chunk of a GLB file is copied once, into its buffer.
    [[nodiscard]] static Reader open(std::span<const std::uint8_t> bytes, std::string_view name, Limits limits = {});

    // Runs every step of reading at once.
    [[nodiscard]] static Document read(const io::Package& package, std::string_view path, Limits limits = {});
    [[nodiscard]] static Document read(std::span<const std::uint8_t> bytes, std::string_view name, Limits limits = {});

    Reader(Reader&&) noexcept;
    Reader& operator=(Reader&&) noexcept;
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
    ~Reader();

    // The paths in the content folder of the package of the files beside the document that its buffers and images name, each once.
    [[nodiscard]] std::vector<std::string> getExternalFiles() const;

    void loadResources();
    [[nodiscard]] Document finish();

  private:
    Reader(const io::Package* source, Limits value);

    // Parses the JSON of the document and checks its shape and its links, for a GLB file or a text file, from a package or from memory.
    void parseDocument(std::string_view text, bool glb, bool filesAllowed);
    void loadBuffer(Buffer& buffer, std::size_t index);
    void loadImage(Image& image, std::size_t index);
    [[nodiscard]] std::vector<std::uint8_t> readFile(const std::string& path, const std::string& owner, std::uint64_t maxBytes, std::uint64_t needed);

    // The media type that the signature of encoded image bytes tells, or nothing for a format that the core and the extensions the reader knows do not define.
    [[nodiscard]] static std::string_view detectMimeType(std::span<const std::uint8_t> bytes) noexcept;
    [[nodiscard]] static bool holds(std::span<const std::uint8_t> bytes, std::size_t offset, std::string_view text) noexcept;

    const io::Package* package;
    Limits limits;
    Document document;

    // Where the binary chunk of a GLB file lies in the file of a package or in the bytes in memory.
    std::unique_ptr<io::PackageReader> file;
    std::span<const std::uint8_t> memory;
    std::uint64_t binaryOffset = 0;
    std::uint64_t binaryLength = 0;
    bool hasBinary = false;
    bool loaded = false;
};

} // namespace haylen::gltf
