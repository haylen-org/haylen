#include "content/ReleasePackage.hpp"

#include <string>
#include <utility>
#include <vector>

#include "content/Error.hpp"
#include "content/HpakPackage.hpp"
#include "content/ShardSet.hpp"
#include "io/CompositePackage.hpp"

namespace haylen::content {

std::unique_ptr<io::Package> ReleasePackage::open(std::shared_ptr<const io::Package> files, std::shared_ptr<const KeyRing> keys, std::span<const VerifyingKey> trustedKeys, const Compatibility& compatibility) {
    std::vector<std::shared_ptr<const io::Package>> layers;
    layers.push_back(mount(files, kAppManifestFile, Manifest::Domain::App, keys, trustedKeys, compatibility));
    layers.push_back(mount(files, kContentManifestFile, Manifest::Domain::Content, keys, trustedKeys, compatibility));
    return std::make_unique<io::CompositePackage>(std::string(files->getName()), std::move(layers));
}

std::shared_ptr<const io::Package> ReleasePackage::mount(const std::shared_ptr<const io::Package>& files, std::string_view manifestFile, Manifest::Domain domain, const std::shared_ptr<const KeyRing>& keys, std::span<const VerifyingKey> trustedKeys, const Compatibility& compatibility) {
    const Manifest manifest = Manifest::read(files->read(manifestFile), trustedKeys);
    if (manifest.getEnvelope().domain != domain) {
        throw Error(Error::Code::CorruptManifest, "The manifest \"" + std::string(manifestFile) + "\" belongs to the other domain.");
    }
    compatibility.check(manifest);

    auto catalog = std::make_shared<const Catalog>(manifest.decryptCatalog(*keys));
    // clang-format off
    auto shards = std::make_shared<const ShardSet>(manifest.getEnvelope().shards, keys, [files](const ShardReference& reference) -> std::unique_ptr<io::PackageReader> {
        const std::string name = reference.getFileName();
        return files->exists(name) ? files->openReader(name) : nullptr;
    });
    // clang-format on
    return std::make_shared<HpakPackage>(std::string(manifestFile), std::move(catalog), std::move(shards));
}

} // namespace haylen::content
