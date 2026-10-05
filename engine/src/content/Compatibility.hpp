#pragma once

#include <cstdint>
#include <string>

#include "content/LuaCompiler.hpp"
#include "content/crypto/Digest.hpp"
#include "content/format/Manifest.hpp"

namespace haylen::content {

// What the running build of an app accepts: manifests of its app and platform profile whose build range includes its build, and Lua bytecode of the ABI of its Lua. The versions of shards and catalogs are checked when a manifest is read.
class Compatibility final {
  public:
    Digest application;
    std::uint64_t appBuild = 0;
    std::string profile;
    std::string luaAbi = LuaCompiler::getAbi();

    // Throws `ManifestIncompatible` with the reason when the manifest is not for this build of the app, and `LuaBytecodeIncompatible` when its Lua bytecode does not load into it.
    void check(const Manifest& manifest) const;
};

} // namespace haylen::content
