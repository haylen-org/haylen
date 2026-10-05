#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace haylen::io {
class Package;
}

namespace haylen::platform {

// The order in which the native parts of the plugins of an app load on the platforms that read it from the package: the order of `app.json`, where every plugin follows the plugins that its `plugin.json` requires, as `haylen.py` orders them for the builds of the other platforms.
class PluginLoadOrder final {
  public:
    // Returns the ids of the plugins that `app.json` of the package lists, in load order. Throws `std::runtime_error` when `app.json` or the `plugin.json` of a listed plugin is not a JSON object.
    [[nodiscard]] static std::vector<std::string> read(const io::Package& package);

  private:
    static void visit(const std::string& id, const std::map<std::string, std::vector<std::string>>& required, std::set<std::string>& visited, std::vector<std::string>& order);
};

} // namespace haylen::platform
