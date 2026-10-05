#pragma once

#include <memory>
#include <span>
#include <string_view>

#include "content/Compatibility.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/crypto/VerifyingKey.hpp"
#include "content/format/Manifest.hpp"
#include "haylen/content/Bootstrap.hpp"
#include "haylen/io/Package.hpp"

namespace haylen::content {

// Opens the protected release of an app as one package, from the files that ship with it: the app manifest, the content manifest and the shards both name. Each manifest is verified and checked against the build of the app before its catalog is decrypted, and the package shows `app.json`, `source` and `plugins` from the app domain and `content` from the content domain, exactly as a folder package would.
class ReleasePackage final {
  public:
    static constexpr std::string_view kAppManifestFile = "app.hmanifest";
    static constexpr std::string_view kContentManifestFile = "content.hmanifest";

    [[nodiscard]] static std::unique_ptr<io::Package> open(std::shared_ptr<const io::Package> files, std::shared_ptr<const KeyRing> keys, std::span<const VerifyingKey> trustedKeys, const Compatibility& compatibility);

    // Opens a release with what the bootstrap of the app holds: the content keys of its provider, which the key ring derives its subkeys from before they are wiped, the public keys it trusts, and its app, build and profile. Throws `KeyUnavailable` when the provider cannot give a key it lists.
    [[nodiscard]] static std::unique_ptr<io::Package> open(std::shared_ptr<const io::Package> files, const Bootstrap& bootstrap);

    // Opens the package that ships with an app: the protected release, with the bootstrap that the release build of the app compiled in, when the files hold an app manifest, or else the files themselves, which development builds ship. Throws `KeyUnavailable` for a release in a build without a bootstrap.
    [[nodiscard]] static std::shared_ptr<io::Package> openBundled(std::shared_ptr<io::Package> files, const Bootstrap* bootstrap);

    // The name of the manifest file of a domain in a release.
    [[nodiscard]] static std::string_view getManifestFile(Manifest::Domain domain) noexcept;

  private:
    [[nodiscard]] static std::shared_ptr<const io::Package> mount(const std::shared_ptr<const io::Package>& files, Manifest::Domain domain, const std::shared_ptr<const KeyRing>& keys, std::span<const VerifyingKey> trustedKeys, const Compatibility& compatibility);
};

} // namespace haylen::content
