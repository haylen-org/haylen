#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace haylen::io {
class Package;
}

namespace haylen::platform {

// The order in which the native parts of the plugins of an app load on the platforms that read it from the package: the order of `haylen.json`, where every plugin follows the plugins that the `dependencies` of its `plugin.json` name.
class PluginLoadOrder final {
  public:
    // Returns the ids of the plugins that `haylen.json` of the package lists, in load order. Throws `std::runtime_error` when `haylen.json` is not a JSON object or the `plugin.json` of a listed plugin cannot be read, as `PluginManifest::read` says.
    [[nodiscard]] static std::vector<std::string> read(const io::Package& package);

  private:
    static void visit(const std::string& id, const std::map<std::string, std::vector<std::string>>& required, std::set<std::string>& visited, std::vector<std::string>& order);
};

} // namespace haylen::platform
