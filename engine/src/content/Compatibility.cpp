#include "content/Compatibility.hpp"

#include <format>

#include "content/Error.hpp"

namespace haylen::content {

void Compatibility::check(const Manifest& manifest) const {
    const Manifest::Envelope& envelope = manifest.getEnvelope();
    const std::string name = manifest.getId().toHex();
    if (envelope.application != application) {
        throw Error(Error::Code::ManifestIncompatible, "The manifest \"" + name + "\" belongs to another app.");
    }
    if (envelope.profile != profile) {
        throw Error(Error::Code::ManifestIncompatible, "The manifest \"" + name + "\" was made for the profile \"" + envelope.profile + "\", and this app runs the profile \"" + profile + "\".");
    }
    if (appBuild < envelope.minimumAppBuild || appBuild > envelope.maximumAppBuild) {
        throw Error(Error::Code::ManifestIncompatible, std::format("The manifest \"{}\" needs an app build from {} to {}, and this app is build {}.", name, envelope.minimumAppBuild, envelope.maximumAppBuild, appBuild));
    }
    if (!envelope.luaAbi.empty() && envelope.luaAbi != luaAbi) {
        throw Error(Error::Code::LuaBytecodeIncompatible, "The manifest \"" + name + "\" holds Lua bytecode of the ABI \"" + envelope.luaAbi + "\", and this build of the app loads \"" + luaAbi + "\". Build the release again with the content tool of this engine.");
    }
}

} // namespace haylen::content
