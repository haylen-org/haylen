#pragma once

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "content/Delivery.hpp"
#include "content/crypto/Digest.hpp"
#include "content/format/Catalog.hpp"

namespace haylen::content {

// Builds the plain bytes of a catalog in its one deterministic order: files sorted by path and chunks sorted by stored ID, whatever order they were added in.
class CatalogWriter final {
  public:
    // Adds a chunk, which the catalog lists once however many files use it.
    void addChunk(const Catalog::Chunk& chunk);

    // Adds a file made of chunks that were added, in order. Throws `std::invalid_argument` for an invalid or repeated path and for chunks that were not added.
    void addFile(std::string path, Delivery delivery, std::span<const Digest> parts);

    [[nodiscard]] std::vector<std::uint8_t> write() const;

  private:
    struct File {
        Delivery delivery = Delivery::Required;
        std::vector<Digest> parts;
    };

    std::map<Digest, Catalog::Chunk> chunks;
    std::map<std::string, File, std::less<>> files;
};

} // namespace haylen::content
