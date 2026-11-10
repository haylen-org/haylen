#pragma once

#include <filesystem>
#include <string>

namespace haylen::platform {

// A face of the system that draws a character: the name that tells it apart from every other face of the system, and the file and the index of the face inside it, which stays empty on platforms that hand over the data of a face instead of its file, as Apple platforms do.
struct SystemFont {
    std::string name;
    std::filesystem::path file;
    int face = 0;
};

} // namespace haylen::platform
