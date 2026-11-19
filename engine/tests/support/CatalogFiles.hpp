#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "content/format/Catalog.hpp"

namespace haylen::test {

// Finds the files of a catalog that a test knows it holds, and fails the test when the catalog lacks one.
class CatalogFiles final {
  public:
    [[nodiscard]] static content::Catalog::File find(const content::Catalog& catalog, std::string_view path) {
        const std::optional<std::uint64_t> index = catalog.findFile(path);
        if (!index) {
            throw std::runtime_error("The catalog holds no file \"" + std::string(path) + "\".");
        }

        return catalog.getFile(*index);
    }
};

} // namespace haylen::test
