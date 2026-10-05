#include "content/ReleasePackage.hpp"

#include <monocypher.h>

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
    layers.push_back(mount(files, Manifest::Domain::App, keys, trustedKeys, compatibility));
    layers.push_back(mount(files, Manifest::Domain::Content, keys, trustedKeys, compatibility));
    return std::make_unique<io::CompositePackage>(std::string(files->getName()), std::move(layers));
}

std::unique_ptr<io::Package> ReleasePackage::open(std::shared_ptr<const io::Package> files, const Bootstrap& bootstrap) {
    auto keys = std::make_shared<KeyRing>();
    for (const KeyProvider::KeyId& id : bootstrap.keys->getKeyIds()) {
        KeyProvider::Key key{};
        const bool provided = bootstrap.keys->getKey(id, key);
        const Digest added = provided ? keys->add(key) : Digest();
        crypto_wipe(key.data(), key.size());
        if (added != Digest(id)) {
            throw Error(Error::Code::KeyUnavailable, "The content key \"" + Digest(id).toHex() + "\" of the app is not available, so the build of the app is damaged. Install the app again.");
        }
    }

    std::vector<VerifyingKey> trusted;
    for (const Bootstrap::PublicKey& key : bootstrap.trustedKeys) {
        trusted.emplace_back(key);
    }
    const Compatibility compatibility{.application = Manifest::identifyApplication(bootstrap.identifier), .appBuild = bootstrap.appBuild, .profile = bootstrap.profile};
    return open(std::move(files), std::move(keys), trusted, compatibility);
}

std::shared_ptr<io::Package> ReleasePackage::openBundled(std::shared_ptr<io::Package> files, const Bootstrap* bootstrap) {
    if (!files->exists(kAppManifestFile)) {
        return files;
    }
    if (bootstrap == nullptr) {
        throw Error(Error::Code::KeyUnavailable, "The app ships a protected release, and this build holds no keys to open it. Build the app in the Release configuration with \"haylen.py\", which compiles the keys of the app into it.");
    }
    return open(std::move(files), *bootstrap);
}

std::string_view ReleasePackage::getManifestFile(Manifest::Domain domain) noexcept {
    return domain == Manifest::Domain::App ? kAppManifestFile : kContentManifestFile;
}

std::shared_ptr<const io::Package> ReleasePackage::mount(const std::shared_ptr<const io::Package>& files, Manifest::Domain domain, const std::shared_ptr<const KeyRing>& keys, std::span<const VerifyingKey> trustedKeys, const Compatibility& compatibility) {
    const std::string_view manifestFile = getManifestFile(domain);
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
