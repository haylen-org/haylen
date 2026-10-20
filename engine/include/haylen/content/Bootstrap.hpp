#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "haylen/content/KeyProvider.hpp"

namespace haylen::content {

// What the release build of an app compiles into its binary to open the protected release it ships with: the identifier of the app, its build number and its content profile, which a release must match, the public keys that sign its manifests and the provider of its content keys. The content tool writes the bootstrap of each app from its key folder, and a static member of the generated code installs it before `main`. It has no Lua binding, since keys never reach Lua.
class Bootstrap final {
  public:
    using PublicKey = std::array<std::uint8_t, 32>;

    std::string identifier;
    std::uint64_t appBuild = 0;
    std::string profile;
    std::vector<PublicKey> trustedKeys;
    std::shared_ptr<const KeyProvider> keys;

    // Makes the bootstrap the one of the running app and returns true, so the static member that calls it holds a value.
    static bool install(Bootstrap bootstrap);

    // Adds a provider after the keys of the installed bootstrap, such as one that obtains the keys of later content from the service of an online game, so the release asks it for every key it lacks. A C++ app calls it in `main` before the engine starts. A build without a bootstrap plays its files as they are and needs no key, so the provider stays unused there.
    static void addKeyProvider(std::shared_ptr<const KeyProvider> provider);

    // Returns the bootstrap of the running app, or null in a build without one, such as the development player.
    [[nodiscard]] static const Bootstrap* find() noexcept;

  private:
    [[nodiscard]] static std::unique_ptr<const Bootstrap>& getInstalled() noexcept;
};

} // namespace haylen::content
