#include <gtest/gtest.h>

#include <filesystem>
#include <vector>

#include "platform/web/WebDialogJson.hpp"

namespace haylen::platform {

TEST(WebDialogJsonTest, DescribesDialogsForThePage) {
    EXPECT_EQ(WebDialogJson::describeMessage({.title = "Unsaved map", .text = "Save it?", .kind = DialogRequest::MessageKind::Warning, .buttons = {"Save", "Discard"}}), (core::Json{{"kind", "message"}, {"title", "Unsaved map"}, {"text", "Save it?"}, {"messageKind", "warning"}, {"buttons", {"Save", "Discard"}}}));

    // The accept attribute lists the extension of every filter with its dot.
    const std::vector<DialogRequest::Filter> filters{{.name = "Images", .extensions = {"png", "jpg"}}, {.name = "Archives", .extensions = {"tar.gz"}}};
    const std::filesystem::path folder = std::filesystem::path("/persistent") / "dev.haylen.sample" / "tmp" / "dialogs" / "7";
    EXPECT_EQ(WebDialogJson::describeOpenFiles({.filters = filters, .multiple = true}, folder), (core::Json{{"kind", "openFiles"}, {"accept", ".png,.jpg,.tar.gz"}, {"multiple", true}, {"folder", "/persistent/dev.haylen.sample/tmp/dialogs/7"}}));
    EXPECT_EQ(WebDialogJson::describeOpenFiles({}, folder).at("accept"), "");

    const core::Json images{{"description", "Images"}, {"accept", {{"application/octet-stream", {".png", ".jpg"}}}}};
    const core::Json archives{{"description", "Archives"}, {"accept", {{"application/octet-stream", {".tar.gz"}}}}};
    EXPECT_EQ(WebDialogJson::describeSaveFile({.filters = filters, .name = "island.png", .data = {1, 2}}), (core::Json{{"kind", "saveFile"}, {"name", "island.png"}, {"types", {images, archives}}}));
    EXPECT_EQ(WebDialogJson::describeSaveFile({.name = "save.json"}).at("types"), core::Json::array());
}

TEST(WebDialogJsonTest, ReadsTheAnswersOfThePage) {
    EXPECT_EQ(WebDialogJson::readAnswer(R"({"button": 2})").button, 2U);

    const DialogResult opened = WebDialogJson::readAnswer(R"({"files": [{"name": "a.png", "path": "/tmp/7/a.png"}, {"name": "b.png", "path": "/tmp/7/b.png"}]})");
    EXPECT_EQ(opened.files, (std::vector<DialogResult::File>{{.name = "a.png", .path = "/tmp/7/a.png"}, {.name = "b.png", .path = "/tmp/7/b.png"}}));

    // A download has no path.
    EXPECT_EQ(WebDialogJson::readAnswer(R"({"saved": {"name": "save.json"}})").saved, (DialogResult::File{.name = "save.json"}));

    const DialogResult failed = WebDialogJson::readAnswer(R"({"failure": {"code": "failed", "message": "The picker needs a click."}})");
    ASSERT_TRUE(failed.failure);
    EXPECT_EQ(failed.failure->code, DialogResult::Code::Failed);
    EXPECT_EQ(failed.failure->message, "The picker needs a click.");

    const DialogResult dismissed = WebDialogJson::readAnswer("{}");
    EXPECT_FALSE(dismissed.button || !dismissed.files.empty() || dismissed.saved || dismissed.folder || dismissed.failure);
}

} // namespace haylen::platform
