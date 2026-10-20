#include "content/ReleasePackage.hpp"

#include <monocypher.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/content/Error.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::content {

ReleasePackage::ReleasePackage(std::shared_ptr<const io::Package> releaseFiles, std::shared_ptr<const KeyRing> contentKeys, std::span<const VerifyingKey> trustedKeys, Compatibility releaseCompatibility, std::shared_ptr<LocalStore> localStore, std::shared_ptr<const KeyProvider> keyProvider) : files(std::move(releaseFiles)), trusted(trustedKeys.begin(), trustedKeys.end()), compatibility(std::move(releaseCompatibility)), store(std::move(localStore)) {
    keys->ring = std::move(contentKeys);
    keys->provider = std::move(keyProvider);
    app = load(files->read(kAppManifestFile), Manifest::Domain::App);
    bundled = load(files->read(kContentManifestFile), Manifest::Domain::Content);
    content = bundled;
    recover();
}

std::unique_ptr<ReleasePackage> ReleasePackage::open(std::shared_ptr<const io::Package> files, std::shared_ptr<const KeyRing> keys, std::span<const VerifyingKey> trustedKeys, const Compatibility& compatibility, std::shared_ptr<LocalStore> store) {
    return std::make_unique<ReleasePackage>(std::move(files), std::move(keys), trustedKeys, compatibility, std::move(store));
}

std::unique_ptr<ReleasePackage> ReleasePackage::open(std::shared_ptr<const io::Package> files, const Bootstrap& bootstrap, std::function<std::unique_ptr<StoreFiles>(std::string_view identifier)> openStore, std::string remoteAddress) {
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

    // The store only holds what downloads can bring again, so a device that refuses it leaves the app with the content it ships.
    std::shared_ptr<LocalStore> store;
    if (openStore) {
        try {
            store = std::make_shared<LocalStore>(openStore(bootstrap.identifier), compatibility.application, compatibility.appBuild);
        } catch (const Error& error) {
            core::Log::warning("The content store of the app could not be opened, so the app plays the content it ships with: {}", error.what());
        }
    }
    auto package = std::make_unique<ReleasePackage>(std::move(files), std::move(keys), trusted, compatibility, std::move(store), bootstrap.keys);
    package->remote = std::move(remoteAddress);
    return package;
}

std::shared_ptr<io::Package> ReleasePackage::openBundled(std::shared_ptr<io::Package> files, const Bootstrap* bootstrap, std::function<std::unique_ptr<StoreFiles>(std::string_view identifier)> openStore, std::string remoteAddress) {
    // A build with the keys of its app plays only its protected release, so plain files in its place never run.
    if (bootstrap != nullptr && !files->exists(kAppManifestFile)) {
        throw Error(Error::Code::MissingManifest, "This build of the app plays only its protected release, and the release is missing or damaged. Install the app again.");
    }
    if (!files->exists(kAppManifestFile)) {
        return files;
    }
    if (bootstrap == nullptr) {
        throw Error(Error::Code::KeyUnavailable, "The app ships a protected release, and this build holds no keys to open it. Build the app in the Release configuration with \"haylen.py\", which compiles the keys of the app into it.");
    }
    return open(std::move(files), *bootstrap, std::move(openStore), std::move(remoteAddress));
}

std::shared_ptr<const KeyRing> ReleasePackage::getKeys() const {
    const std::scoped_lock lock(keys->mutex);
    return keys->ring;
}

void ReleasePackage::acquireKey(const Digest& id, std::function<void(bool)> done) {
    std::shared_ptr<const KeyProvider> provider;
    {
        const std::scoped_lock lock(keys->mutex);
        if (keys->ring->contains(id)) {
            done(true);
            return;
        }
        provider = keys->provider;
    }
    if (!provider) {
        done(false);
        return;
    }
    KeyProvider::KeyId wanted{};
    std::ranges::copy(id.getBytes(), wanted.begin());
    // clang-format off
    provider->requestKey(wanted, [shared = keys, provider, wanted, id, done = std::move(done)](bool given) {
        KeyProvider::Key key{};
        if (!given || !provider->getKey(wanted, key)) {
            done(false);
            return;
        }
        auto ring = std::make_shared<KeyRing>();
        {
            const std::scoped_lock lock(shared->mutex);
            *ring = *shared->ring;
            ring->add(key);
            crypto_wipe(key.data(), key.size());
            if (!ring->contains(id)) {
                done(false);
                return;
            }
            shared->ring = std::move(ring);
        }
        done(true);
    });
    // clang-format on
}

std::string_view ReleasePackage::getManifestFile(Manifest::Domain domain) noexcept {
    return domain == Manifest::Domain::App ? kAppManifestFile : kContentManifestFile;
}

std::shared_ptr<const ReleasePackage::Content> ReleasePackage::load(std::vector<std::uint8_t> manifestBytes) const {
    return load(std::move(manifestBytes), Manifest::Domain::Content);
}

std::shared_ptr<const ReleasePackage::Content> ReleasePackage::load(std::vector<std::uint8_t> manifestBytes, Manifest::Domain domain) const {
    const std::shared_ptr<const KeyRing> ring = getKeys();
    auto manifest = std::make_shared<const Manifest>(Manifest::read(std::move(manifestBytes), trusted));
    if (manifest->getEnvelope().domain != domain) {
        throw Error(Error::Code::CorruptManifest, "The manifest \"" + manifest->getId().toHex() + "\" belongs to the other domain.", {.manifestId = manifest->getId().toHex()});
    }
    compatibility.check(*manifest);

    std::shared_ptr<const Catalog> catalog;
    try {
        catalog = std::make_shared<const Catalog>(manifest->decryptCatalog(*ring));
    } catch (Error& error) {
        error.getDetails().manifestId = manifest->getId().toHex();
        throw;
    }
    // clang-format off
    auto shards = std::make_shared<const ShardSet>(manifest->getEnvelope().shards, ring, [releaseFiles = files, packFolders = packs](const ShardReference& reference) -> std::unique_ptr<io::PackageReader> {
        const std::string name = reference.getFileName();
        if (releaseFiles->exists(name)) {
            return releaseFiles->openReader(name);
        }
        std::scoped_lock lock(packFolders->mutex);
        for (const std::shared_ptr<const io::Package>& folder : packFolders->folders) {
            if (folder->exists(name)) {
                return folder->openReader(name);
            }
        }
        return nullptr;
    }, domain == Manifest::Domain::Content ? store : nullptr);
    // clang-format on
    auto package = std::make_shared<const HpakPackage>(std::string(getManifestFile(domain)), catalog, shards);
    return std::make_shared<const Content>(Content{.manifest = std::move(manifest), .catalog = std::move(catalog), .shards = std::move(shards), .package = std::move(package)});
}

std::vector<bool> ReleasePackage::findInstalledShards(const Content& shown) const {
    const std::vector<ShardReference>& references = shown.shards->getReferences();
    std::vector<bool> installed(references.size());
    const std::scoped_lock lock(packs->mutex);
    for (std::size_t index = 0; index < references.size(); ++index) {
        const std::string name = references[index].getFileName();
        installed[index] = files->exists(name) || std::ranges::any_of(packs->folders, [&name](const std::shared_ptr<const io::Package>& pack) { return pack->exists(name); });
    }
    return installed;
}

void ReleasePackage::addPackFolder(const std::filesystem::path& folder) {
    if (!std::filesystem::is_directory(folder)) {
        throw std::runtime_error("The pack folder \"" + folder.generic_string() + "\" does not exist.");
    }
    {
        std::scoped_lock lock(packs->mutex);
        packs->folders.push_back(io::Package::openDirectory(folder));
    }

    // The shards that were missing before may be in the new folder, and the content shipped with the app shares the shard files with downloaded content.
    getContent()->shards->forgetMissing();
    bundled->shards->forgetMissing();
}

void ReleasePackage::recover() {
    if (!store) {
        return;
    }
    StoreState state = store->getState();
    for (const Digest& id : {state.active, state.previous}) {
        if (id.isZero()) {
            break;
        }
        try {
            std::optional<std::vector<std::uint8_t>> bytes = store->readManifest(id);
            if (!bytes) {
                throw Error(Error::Code::MissingManifest, "The store lost the manifest.");
            }
            std::shared_ptr<const Content> loaded = load(std::move(*bytes));
            if (loaded->manifest->getId() != id) {
                throw Error(Error::Code::CorruptManifest, "The file of the manifest holds another manifest.");
            }
            content = std::move(loaded);
            break;
        } catch (const Error& error) {
            // Content under a key that the provider does not hold yet stays in the store and plays from a start at which the provider holds it.
            if (error.getCode() == Error::Code::UnknownKeyId) {
                core::Log::warning("The content \"{}\" waits for its key, so the app plays earlier content: {}", id.toHex(), error.what());
                continue;
            }
            if (!recovery) {
                recovery = Recovery{.failed = id, .reason = error.what()};
            }
        }
    }
    if (!recovery) {
        return;
    }

    // The content that failed never plays again, so the store moves on to what plays now.
    core::Log::warning("The content \"{}\" that the app played last cannot play again, so it falls back to earlier content: {}", recovery->failed.toHex(), recovery->reason);
    state.active = content == bundled ? Digest() : content->manifest->getId();
    state.previous = Digest();
    try {
        store->commit(state);
    } catch (const Error& error) {
        core::Log::warning("The content store could not record the fallback: {}", error.what());
    }
}

void ReleasePackage::mount(std::shared_ptr<const Content> next) {
    std::scoped_lock lock(mutex);
    content = std::move(next);
}

std::shared_ptr<const ReleasePackage::Content> ReleasePackage::getContent() const {
    std::scoped_lock lock(mutex);
    return content;
}

std::shared_ptr<const io::Package> ReleasePackage::getDomain(std::string_view path) const {
    const std::string normalized = io::Path::normalize(path);
    if (normalized == io::Path::kContentDirectory || normalized.starts_with(std::string(io::Path::kContentDirectory) + "/")) {
        return getContent()->package;
    }
    return app->package;
}

std::string_view ReleasePackage::getName() const noexcept {
    return files->getName();
}

bool ReleasePackage::exists(std::string_view path) const {
    return getDomain(path)->exists(path);
}

std::uint64_t ReleasePackage::getFileSize(std::string_view path) const {
    return getDomain(path)->getFileSize(path);
}

std::unique_ptr<io::PackageReader> ReleasePackage::openReader(std::string_view path) const {
    return getDomain(path)->openReader(path);
}

bool ReleasePackage::isLuaBytecode(std::string_view path) const {
    return getDomain(path)->isLuaBytecode(path);
}

bool ReleasePackage::isLocal(std::string_view path) const {
    const std::shared_ptr<const Content> shown = io::Path::isInside(io::Path::normalize(path), io::Path::kContentDirectory) ? getContent() : app;
    const std::optional<std::uint64_t> index = shown->catalog->findFile(io::Path::normalize(path));
    if (!index) {
        throw std::runtime_error("The package file \"" + io::Path::normalize(path) + "\" was not found.");
    }
    const Catalog::File file = shown->catalog->getFile(*index);
    for (std::uint64_t part = file.firstPart; part < file.firstPart + file.partCount; ++part) {
        if (!shown->shards->hasChunk(shown->catalog->getChunk(shown->catalog->getPart(part).chunk))) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> ReleasePackage::list(std::string_view directory) const {
    if (!io::Path::normalize(directory).empty()) {
        return getDomain(directory)->list(directory);
    }
    std::vector<std::string> paths = app->package->list(directory);
    std::vector<std::string> assets = getContent()->package->list(directory);
    paths.insert(paths.end(), std::make_move_iterator(assets.begin()), std::make_move_iterator(assets.end()));
    std::ranges::sort(paths);
    return paths;
}

} // namespace haylen::content
