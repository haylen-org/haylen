#pragma once

#include <memory>
#include <string>
#include <vector>

#include "haylen/io/Package.hpp"

namespace haylen::io {

// Shows several packages as one. A file that more than one layer holds comes from the last of them, so later layers patch earlier ones, and listings merge every layer.
class CompositePackage final : public Package {
  public:
    CompositePackage(std::string packageName, std::vector<std::shared_ptr<const Package>> packageLayers);

    [[nodiscard]] std::string_view getName() const noexcept override {
        return name;
    }
    [[nodiscard]] bool exists(std::string_view path) const override;
    [[nodiscard]] std::uint64_t getFileSize(std::string_view path) const override;
    [[nodiscard]] std::unique_ptr<PackageReader> openReader(std::string_view path) const override;
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const override;

  private:
    [[nodiscard]] const Package& find(std::string_view path) const;

    std::string name;
    std::vector<std::shared_ptr<const Package>> layers;
};

} // namespace haylen::io
