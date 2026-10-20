#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::assets {

// The format of preload groups: a `groups` object that maps each group name to its list of entries, where an entry is a path relative to `content` or an object with `path` and an optional `type` and `options`. A path that ends with `/` names a folder. The asset manager loads the groups, and the delivery rules of `app.json` name them.
class GroupManifest final {
  public:
    struct Entry {
        std::string path;
        std::string type;
        core::Json options = core::Json::object();
    };

    // Reads every group of a manifest by its name. Throws `std::invalid_argument` with the reason for a manifest that does not follow the format.
    [[nodiscard]] static std::map<std::string, std::vector<Entry>> read(const core::Json& manifest);

  private:
    [[nodiscard]] static bool isList(const core::Json& value) noexcept;
    [[nodiscard]] static Entry readEntry(std::string_view group, const core::Json& item);
};

} // namespace haylen::assets
