#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/platform/DialogRequest.hpp"
#include "haylen/platform/DialogResult.hpp"

namespace haylen::platform {

// The native dialogs of the web runtime as the JSON that `platform/web/haylen-runtime.js` shows them from and answers them with. It compiles on every platform, so the tests of every host check it.
class WebDialogJson final {
  public:
    [[nodiscard]] static core::Json describeMessage(const DialogRequest::Message& message);

    // The file picker with the `accept` attribute of the filters, such as `.png,.jpg`, and the folder of the file system that the page copies the picked files into.
    [[nodiscard]] static core::Json describeOpenFiles(const DialogRequest::OpenFiles& files, const std::filesystem::path& folder);

    // The save picker with the suggested name and the `types` option of `showSaveFilePicker`. The data travels apart, as bytes.
    [[nodiscard]] static core::Json describeSaveFile(const DialogRequest::SaveFile& save);

    // Reads an answer of the page: `{"button": index}`, `{"files": [{name, path}]}`, `{"saved": {name, path}}` with an optional path, `{"failure": {code, message}}`, or `{}` for a dialog the user dismissed.
    [[nodiscard]] static DialogResult readAnswer(std::string_view json);

  private:
    [[nodiscard]] static std::string getAccept(const std::vector<DialogRequest::Filter>& filters);
    [[nodiscard]] static DialogResult::File readFile(const core::Json& file);
};

} // namespace haylen::platform
