#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "content/KeyStore.hpp"

namespace haylen::content {

// Writes the bootstrap that the release build of an app compiles in, as C++ source: the identifier, build and content profile of the app, the public key that signs its manifests and its content keys sealed for `EmbeddedKeyProvider`, as integer constants that hold no key, with the static member that installs it before `main`. The source belongs to one build of one app and lives outside every repository.
class BootstrapWriter final {
  public:
    [[nodiscard]] static std::string write(const KeyStore& keys, std::string_view profile, std::uint64_t appBuild);

  private:
    [[nodiscard]] static std::string writeBytes(std::span<const std::uint8_t> bytes);
    [[nodiscard]] static std::string writeText(std::string_view text);
};

} // namespace haylen::content
