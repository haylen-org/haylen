#pragma once

#include <memory>
#include <span>
#include <string_view>

#include "content/Compatibility.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/crypto/VerifyingKey.hpp"
#include "content/format/Manifest.hpp"
#include "haylen/io/Package.hpp"

namespace haylen::content {

// Opens the protected release of an app as one package, from the files that ship with it: the app manifest, the content manifest and the shards both name. Each manifest is verified and checked against the build of the app before its catalog is decrypted, and the package shows `app.json`, `source` and `plugins` from the app domain and `content` from the content domain, exactly as a folder package would.
class ReleasePackage final {
  public:
    static constexpr std::string_view kAppManifestFile = "app.hmanifest";
    static constexpr std::string_view kContentManifestFile = "content.hmanifest";

    [[nodiscard]] static std::unique_ptr<io::Package> open(std::shared_ptr<const io::Package> files, std::shared_ptr<const KeyRing> keys, std::span<const VerifyingKey> trustedKeys, const Compatibility& compatibility);

  private:
    [[nodiscard]] static std::shared_ptr<const io::Package> mount(const std::shared_ptr<const io::Package>& files, std::string_view manifestFile, Manifest::Domain domain, const std::shared_ptr<const KeyRing>& keys, std::span<const VerifyingKey> trustedKeys, const Compatibility& compatibility);
};

} // namespace haylen::content
