#include <gtest/gtest.h>

#include <filesystem>
#include <vector>

#include "platform/android/AndroidDialogJson.hpp"

namespace haylen::platform {

TEST(AndroidDialogJsonTest, DescribesDialogsForTheJavaSide) {
    EXPECT_EQ(AndroidDialogJson::describe(DialogRequest::Message{.title = "Unsaved map", .text = "Save it?", .kind = DialogRequest::MessageKind::Warning, .buttons = {"Save", "Discard", "Cancel"}}), (core::Json{{"kind", "message"}, {"title", "Unsaved map"}, {"text", "Save it?"}, {"messageKind", "warning"}, {"buttons", {"Save", "Discard", "Cancel"}}}));

    // Every extension of the filters goes once, in their order, and the Java side turns them into MIME types.
    const std::vector<DialogRequest::Filter> filters{{.name = "Images", .extensions = {"png", "jpg"}}, {.name = "Pictures", .extensions = {"jpg", "webp"}}};
    const std::filesystem::path folder = std::filesystem::path("/data/user/0/dev.haylen.sample/files/dev.haylen.sample/tmp/dialogs") / "7";
    EXPECT_EQ(AndroidDialogJson::describe(DialogRequest::OpenFiles{.title = "Pick", .filters = filters, .multiple = true}, folder), (core::Json{{"kind", "openFiles"}, {"title", "Pick"}, {"extensions", {"png", "jpg", "webp"}}, {"multiple", true}, {"folder", "/data/user/0/dev.haylen.sample/files/dev.haylen.sample/tmp/dialogs/7"}}));
    EXPECT_EQ(AndroidDialogJson::describe(DialogRequest::OpenFiles{}, folder).at("extensions"), core::Json::array());
    EXPECT_EQ(AndroidDialogJson::describe(DialogRequest::SaveFile{.filters = filters, .name = "island.png", .data = {1, 2}}), (core::Json{{"kind", "saveFile"}, {"title", ""}, {"name", "island.png"}, {"extensions", {"png", "jpg", "webp"}}}));
}

TEST(AndroidDialogJsonTest, ReadsTheCopiesOfThePickedFiles) {
    EXPECT_EQ(AndroidDialogJson::readFiles(R"([{"name": "a.png", "path": "/tmp/7/a.png"}, {"name": "a.png", "path": "/tmp/7/2/a.png"}])"), (std::vector<DialogResult::File>{{.name = "a.png", .path = "/tmp/7/a.png"}, {.name = "a.png", .path = "/tmp/7/2/a.png"}}));
    EXPECT_TRUE(AndroidDialogJson::readFiles("[]").empty());
}

} // namespace haylen::platform
