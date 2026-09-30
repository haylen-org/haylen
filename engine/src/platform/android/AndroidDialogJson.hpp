#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/platform/DialogRequest.hpp"
#include "haylen/platform/DialogResult.hpp"

namespace haylen::platform {

// The native dialogs of the Android host as the JSON that `HaylenDialogs` of the Java side shows them from and answers the picked files with. The Java side turns the extensions of the filters into the MIME types of the Storage Access Framework. It compiles on every platform, so the tests of every host check it.
class AndroidDialogJson final {
  public:
    [[nodiscard]] static core::Json describe(const DialogRequest::Message& message);

    // The picker of files to open, with the folder that the Java side copies the picked files into.
    [[nodiscard]] static core::Json describe(const DialogRequest::OpenFiles& files, const std::filesystem::path& folder);

    // The picker of the destination of a save with the suggested name. The data travels apart, as bytes.
    [[nodiscard]] static core::Json describe(const DialogRequest::SaveFile& save);

    // Reads the copies of the picked files, a list of `{name, path}`.
    [[nodiscard]] static std::vector<DialogResult::File> readFiles(std::string_view json);

  private:
    // Every extension of the filters once, in their order.
    [[nodiscard]] static std::vector<std::string> getExtensions(const std::vector<DialogRequest::Filter>& filters);
};

} // namespace haylen::platform
