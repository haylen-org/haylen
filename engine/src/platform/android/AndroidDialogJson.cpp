#include "platform/android/AndroidDialogJson.hpp"

#include <algorithm>

namespace haylen::platform {

core::Json AndroidDialogJson::describe(const DialogRequest::Message& message) {
    return {{"kind", "message"}, {"title", message.title}, {"text", message.text}, {"messageKind", DialogRequest::messageKindName(message.kind)}, {"buttons", message.buttons}};
}

core::Json AndroidDialogJson::describe(const DialogRequest::OpenFiles& files, const std::filesystem::path& folder) {
    return {{"kind", "openFiles"}, {"title", files.title}, {"extensions", getExtensions(files.filters)}, {"multiple", files.multiple}, {"folder", folder.generic_string()}};
}

core::Json AndroidDialogJson::describe(const DialogRequest::SaveFile& save) {
    return {{"kind", "saveFile"}, {"title", save.title}, {"name", save.name}, {"extensions", getExtensions(save.filters)}};
}

std::vector<DialogResult::File> AndroidDialogJson::readFiles(std::string_view json) {
    std::vector<DialogResult::File> files;
    for (const core::Json& file : core::Json::parse(json)) {
        files.push_back({.name = file.at("name").get<std::string>(), .path = file.at("path").get<std::string>()});
    }
    return files;
}

std::vector<std::string> AndroidDialogJson::getExtensions(const std::vector<DialogRequest::Filter>& filters) {
    std::vector<std::string> extensions;
    for (const DialogRequest::Filter& filter : filters) {
        for (const std::string& extension : filter.extensions) {
            if (std::ranges::find(extensions, extension) == extensions.end()) {
                extensions.push_back(extension);
            }
        }
    }
    return extensions;
}

} // namespace haylen::platform
