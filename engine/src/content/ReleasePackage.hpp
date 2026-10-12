#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "content/Compatibility.hpp"
#include "content/HpakPackage.hpp"
#include "content/LocalStore.hpp"
#include "content/ShardSet.hpp"
#include "content/StoreFiles.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/crypto/VerifyingKey.hpp"
#include "content/format/Catalog.hpp"
#include "content/format/Manifest.hpp"
#include "haylen/content/Bootstrap.hpp"
#include "haylen/io/Package.hpp"

namespace haylen::content {

// The protected release of an app as one package, from the files that ship with it: the app manifest, the content manifest and the shards both name. Each manifest is verified and checked against the build of the app before its catalog is decrypted, and the package shows `app.json`, `source` and `plugins` from the app domain and `content` from the content domain, exactly as a folder package would. With a local store, the content domain may be an update that downloads brought instead of the content that ships with the app: opening picks the content the app played last when it is still whole, and falls back to the one before it and then to the content that ships with the app, so damaged downloads never keep an app from starting.
class ReleasePackage final : public io::Package {
  public:
    static constexpr std::string_view kAppManifestFile = "app.hmanifest";
    static constexpr std::string_view kContentManifestFile = "content.hmanifest";

    // A content manifest the package can show: the verified manifest, its decrypted catalog, the chunks it reads and the virtual package of its files.
    struct Content {
        std::shared_ptr<const Manifest> manifest;
        std::shared_ptr<const Catalog> catalog;
        std::shared_ptr<const ShardSet> shards;
        std::shared_ptr<const HpakPackage> package;
    };

    // Why the content the app played last could not be played again, which made the package fall back to earlier content.
    struct Recovery {
        Digest failed;
        std::string reason;
    };

    ReleasePackage(std::shared_ptr<const io::Package> releaseFiles, std::shared_ptr<const KeyRing> contentKeys, std::span<const VerifyingKey> trustedKeys, Compatibility releaseCompatibility, std::shared_ptr<LocalStore> localStore);

    // Opens a release from its files, with the local store of its downloads or without one.
    [[nodiscard]] static std::unique_ptr<ReleasePackage> open(std::shared_ptr<const io::Package> files, std::shared_ptr<const KeyRing> keys, std::span<const VerifyingKey> trustedKeys, const Compatibility& compatibility, std::shared_ptr<LocalStore> store = nullptr);

    // Opens a release with what the bootstrap of the app holds: the content keys of its provider, which the key ring derives its subkeys from before they are wiped, the public keys it trusts, and its app, build and profile, and with the local store in the files given, when any. A store that the device refuses to open leaves the release without one, which plays the content that ships with the app. Throws `KeyUnavailable` when the provider cannot give a key it lists.
    [[nodiscard]] static std::unique_ptr<ReleasePackage> open(std::shared_ptr<const io::Package> files, const Bootstrap& bootstrap, std::function<std::unique_ptr<StoreFiles>(std::string_view identifier)> openStore = {}, std::string remoteAddress = {});

    // Opens the package that ships with an app: the protected release, with the bootstrap that the release build of the app compiled in, when the files hold an app manifest, or else the files themselves, which development builds ship. Throws `KeyUnavailable` for a release in a build without a bootstrap.
    [[nodiscard]] static std::shared_ptr<io::Package> openBundled(std::shared_ptr<io::Package> files, const Bootstrap* bootstrap, std::function<std::unique_ptr<StoreFiles>(std::string_view identifier)> openStore = {}, std::string remoteAddress = {});

    // The name of the manifest file of a domain in a release.
    [[nodiscard]] static std::string_view getManifestFile(Manifest::Domain domain) noexcept;

    [[nodiscard]] std::string_view getName() const noexcept override;
    [[nodiscard]] bool exists(std::string_view path) const override;
    [[nodiscard]] std::uint64_t getFileSize(std::string_view path) const override;
    [[nodiscard]] std::unique_ptr<io::PackageReader> openReader(std::string_view path) const override;
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const override;
    [[nodiscard]] bool isLuaBytecode(std::string_view path) const override;

    // Verifies a content manifest of the app, checks it against the build and decrypts its catalog, ready to mount. Throws the error of the check that fails.
    [[nodiscard]] std::shared_ptr<const Content> load(std::vector<std::uint8_t> manifestBytes) const;

    // Shows another content manifest from now on. Readers that are open keep reading the content they opened.
    void mount(std::shared_ptr<const Content> content);

    [[nodiscard]] std::shared_ptr<const Content> getContent() const;
    [[nodiscard]] const std::shared_ptr<const Content>& getBundledContent() const noexcept {
        return bundled;
    }
    [[nodiscard]] const std::shared_ptr<LocalStore>& getStore() const noexcept {
        return store;
    }
    [[nodiscard]] const std::shared_ptr<const KeyRing>& getKeys() const noexcept {
        return keys;
    }
    [[nodiscard]] std::span<const VerifyingKey> getTrustedKeys() const noexcept {
        return trusted;
    }
    [[nodiscard]] const Compatibility& getCompatibility() const noexcept {
        return compatibility;
    }
    [[nodiscard]] const std::shared_ptr<const io::Package>& getFiles() const noexcept {
        return files;
    }

    // The address that the shards of the release download from, ending with a slash, when the device holds only its manifests and its app domain, as a web page does. Empty when the release ships whole.
    [[nodiscard]] const std::string& getRemoteAddress() const noexcept {
        return remote;
    }
    // Returns why the opening fell back to earlier content, once, so the app hears of it at its first start only.
    [[nodiscard]] std::optional<Recovery> takeRecovery() noexcept {
        return std::exchange(recovery, std::nullopt);
    }

    // Whether every chunk of a file of the package is on the device. Throws `std::runtime_error` for a file that does not exist.
    [[nodiscard]] bool isLocal(std::string_view path) const;

    // Reads shards also from a folder where a store installed the shards of a pack while the app runs, so the assets of the pack load from then on. A shard is verified against its manifest like any other, so a folder never brings content that the release does not name. Throws `std::runtime_error` for a path that is no folder.
    void addPackFolder(const std::filesystem::path& folder);

  private:
    // The folders that stores installed packs into, which every shard set of the release reads from.
    struct PackFolders {
        std::mutex mutex;
        std::vector<std::shared_ptr<const io::Package>> folders;
    };

    [[nodiscard]] std::shared_ptr<const Content> load(std::vector<std::uint8_t> manifestBytes, Manifest::Domain domain) const;
    [[nodiscard]] std::shared_ptr<const io::Package> getDomain(std::string_view path) const;

    // Mounts the content the store names as active, or the one before it, or the content that ships with the app, and records why it fell back.
    void recover();

    std::shared_ptr<const io::Package> files;
    std::shared_ptr<PackFolders> packs = std::make_shared<PackFolders>();
    std::shared_ptr<const KeyRing> keys;
    std::vector<VerifyingKey> trusted;
    Compatibility compatibility;
    std::shared_ptr<LocalStore> store;
    std::shared_ptr<const Content> app;
    std::shared_ptr<const Content> bundled;
    mutable std::mutex mutex;
    std::shared_ptr<const Content> content;
    std::optional<Recovery> recovery;
    std::string remote;
};

} // namespace haylen::content
