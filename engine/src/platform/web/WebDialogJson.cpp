#include "platform/web/WebDialogJson.hpp"

namespace haylen::platform {

core::Json WebDialogJson::describeMessage(const DialogRequest::Message& message) {
    return {{"kind", "message"}, {"title", message.title}, {"text", message.text}, {"messageKind", DialogRequest::messageKindName(message.kind)}, {"buttons", message.buttons}};
}

core::Json WebDialogJson::describeOpenFiles(const DialogRequest::OpenFiles& files, const std::filesystem::path& folder) {
    return {{"kind", "openFiles"}, {"accept", getAccept(files.filters)}, {"multiple", files.multiple}, {"folder", folder.generic_string()}};
}

// The picker lists the types by their extensions, and every type needs a MIME type, which extensions alone do not tell, so all of them name bytes.
core::Json WebDialogJson::describeSaveFile(const DialogRequest::SaveFile& save) {
    core::Json types = core::Json::array();
    for (const DialogRequest::Filter& filter : save.filters) {
        core::Json extensions = core::Json::array();
        for (const std::string& extension : filter.extensions) {
            extensions.push_back("." + extension);
        }
        types.push_back({{"description", filter.name}, {"accept", {{"application/octet-stream", std::move(extensions)}}}});
    }
    return {{"kind", "saveFile"}, {"name", save.name}, {"types", std::move(types)}};
}

DialogResult WebDialogJson::readAnswer(std::string_view json) {
    const core::Json answer = core::Json::parse(json);
    DialogResult result;
    if (answer.contains("failure")) {
        const core::Json& failure = answer.at("failure");
        result.failure = DialogResult::Failure{.code = DialogResult::codeFromName(failure.at("code").get<std::string>()).value(), .message = failure.at("message").get<std::string>()};
        return result;
    }

    if (answer.contains("button")) {
        result.button = answer.at("button").get<std::size_t>();
    }
    if (answer.contains("files")) {
        for (const core::Json& file : answer.at("files")) {
            result.files.push_back(readFile(file));
        }
    }
    if (answer.contains("saved")) {
        result.saved = readFile(answer.at("saved"));
    }
    return result;
}

std::string WebDialogJson::getAccept(const std::vector<DialogRequest::Filter>& filters) {
    std::string accept;
    for (const DialogRequest::Filter& filter : filters) {
        for (const std::string& extension : filter.extensions) {
            accept += (accept.empty() ? "." : ",.") + extension;
        }
    }
    return accept;
}

DialogResult::File WebDialogJson::readFile(const core::Json& file) {
    return {.name = file.at("name").get<std::string>(), .path = file.value("path", std::string())};
}

} // namespace haylen::platform
