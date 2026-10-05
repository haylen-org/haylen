#pragma once

#include <cstdint>
#include <string>

#include "content/crypto/Digest.hpp"
#include "content/format/Manifest.hpp"

namespace haylen::content {

// What the running build of an app accepts: manifests of its app and platform profile whose build range includes its build. The versions of shards and catalogs are checked when a manifest is read.
class Compatibility final {
  public:
    Digest application;
    std::uint64_t appBuild = 0;
    std::string profile;

    // Throws `ManifestIncompatible` with the reason when the manifest is not for this build of the app.
    void check(const Manifest& manifest) const;
};

} // namespace haylen::content
