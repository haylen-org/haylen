#pragma once

#include <string>
#include <string_view>

namespace haylen::platform {

// The glob pattern that the GTK file choosers match the files of an extension with. It ignores case, as the file dialogs of the other desktops do. It compiles on every platform, so the tests of every host check it.
class LinuxFilePattern final {
  public:
    // Returns `*.[pP][nN][gG]` for `png`.
    [[nodiscard]] static std::string fromExtension(std::string_view extension);
};

} // namespace haylen::platform
