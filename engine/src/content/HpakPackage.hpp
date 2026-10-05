#pragma once

#include <memory>
#include <string>

#include "content/ShardSet.hpp"
#include "content/format/Catalog.hpp"
#include "haylen/io/Package.hpp"

namespace haylen::content {

// The virtual package of one manifest: the paths of its decrypted catalog over the chunks of its shards. Lookups and listings read the catalog alone, and readers decrypt only the chunks that cover the ranges they read, so no file and no shard is ever loaded whole.
class HpakPackage final : public io::Package {
  public:
    HpakPackage(std::string packageName, std::shared_ptr<const Catalog> packageCatalog, std::shared_ptr<const ShardSet> packageShards);

    [[nodiscard]] std::string_view getName() const noexcept override {
        return name;
    }
    [[nodiscard]] bool exists(std::string_view path) const override;
    [[nodiscard]] std::uint64_t getFileSize(std::string_view path) const override;
    [[nodiscard]] std::unique_ptr<io::PackageReader> openReader(std::string_view path) const override;
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const override;
    [[nodiscard]] bool isLuaBytecode(std::string_view path) const override;

  private:
    [[nodiscard]] Catalog::File find(std::string_view path) const;

    std::string name;
    std::shared_ptr<const Catalog> catalog;
    std::shared_ptr<const ShardSet> shards;
};

} // namespace haylen::content
