#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/platform/Dialogs.hpp"
#include "platform/DialogRelay.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::platform {

// Dialogs on a headless host of their own, with the answers their callbacks received as text.
class DialogsTest : public ::testing::Test {
  protected:
    [[nodiscard]] Dialogs::Callback record() {
        return [this](DialogResult result) { answers.push_back(describe(result)); };
    }

    [[nodiscard]] static std::string describe(const DialogResult& result) {
        if (result.failure) {
            return std::string(DialogResult::codeName(result.failure->code)) + ": " + result.failure->message;
        }
        if (result.button) {
            return "button " + std::to_string(*result.button);
        }
        if (!result.files.empty()) {
            return "files " + std::to_string(result.files.size());
        }
        return result.folder ? "folder " + *result.folder : "dismissed";
    }

    [[nodiscard]] static DialogRequest message(std::vector<std::string> buttons) {
        return {.dialog = DialogRequest::Message{.text = "Leave the island?", .buttons = std::move(buttons)}};
    }

    test::TemporaryDirectory directory;
    HeadlessHost host{directory.getPath()};
    std::vector<std::string> answers;
};

TEST_F(DialogsTest, RefusesRequestsThePlatformCannotShow) {
    Dialogs dialogs(host, directory.getPath() / "dialogs");
    // clang-format off
    const auto refuses = [&dialogs](const DialogRequest& request, const std::string& reason) {
        try {
            dialogs.show(request, {});
            ADD_FAILURE() << "The request was shown: " << reason;
        } catch (const std::invalid_argument& error) {
            EXPECT_EQ(error.what(), reason);
        }
    };
    // clang-format on
    refuses({.dialog = DialogRequest::Message{.buttons = {"OK"}}}, "A message dialog needs a text.");
    refuses(message({}), "A message dialog has one to three buttons, not 0.");
    refuses(message({"Yes", "No", "Later", "Never"}), "A message dialog has one to three buttons, not 4.");
    refuses(message({"Yes", ""}), "Every button of a message dialog needs a label.");
    refuses({.dialog = DialogRequest::OpenFiles{.filters = {{.extensions = {"png"}}}}}, "Every filter of a file dialog needs a name.");
    refuses({.dialog = DialogRequest::OpenFiles{.filters = {{.name = "Images"}}}}, "The filter \"Images\" needs at least one extension.");
    for (const std::string extension : {".png", "*.png", "images/png", "png;jpg", ""}) {
        refuses({.dialog = DialogRequest::OpenFiles{.filters = {{.name = "Images", .extensions = {"jpg", extension}}}}}, "The extension \"" + extension + "\" of the filter \"Images\" is invalid. Extensions come without their dot, such as \"png\" or \"tar.gz\".");
    }
    refuses({.dialog = DialogRequest::SaveFile{.data = {1}}}, "A save dialog needs the name it suggests for the file.");
    refuses({.dialog = DialogRequest::SaveFile{.name = "saves/slot.json"}}, "The name \"saves/slot.json\" that a save dialog suggests is a file name, without folders.");
    EXPECT_THROW(dialogs.show(message({"OK"}), {}, std::chrono::seconds(0)), std::invalid_argument);
    EXPECT_TRUE(host.getDialogRequests().empty());
    EXPECT_EQ(dialogs.getPendingCount(), 0U);

    dialogs.show({.dialog = DialogRequest::OpenFiles{.filters = {{.name = "Archives", .extensions = {"tar.gz", "zip"}}}, .multiple = true}}, {});
    dialogs.show({.dialog = DialogRequest::SaveFile{.name = "slot.json"}}, {});
    dialogs.show({.dialog = DialogRequest::OpenFolder{}}, {});
    EXPECT_EQ(host.getDialogRequests().size(), 3U);
}

TEST_F(DialogsTest, AnswersOnceAtTheNextPumpFromAnyThread) {
    Dialogs dialogs(host, directory.getPath() / "dialogs");
    const std::uint64_t asked = dialogs.show(message({"Stay", "Leave"}), record());
    const std::uint64_t other = dialogs.show({.dialog = DialogRequest::OpenFolder{.title = "Export to"}}, record());
    ASSERT_EQ(host.getDialogRequests().size(), 2U);
    const HeadlessHost::DialogCall& shown = host.getDialogRequests().front();
    EXPECT_EQ(shown.id, asked);
    EXPECT_EQ(shown.folder, directory.getPath() / "dialogs" / std::to_string(asked));
    EXPECT_EQ(std::get<DialogRequest::Message>(shown.request.dialog).buttons, (std::vector<std::string>{"Stay", "Leave"}));
    EXPECT_NE(asked, other);

    // clang-format off
    std::thread([&] {
        dialogs.resolve(asked, {.button = 1});
        dialogs.resolve(asked, {.button = 0});
        dialogs.resolve(other + 1000, {.button = 0});
        dialogs.resolve(other, {.folder = "/exports"});
    }).join();
    // clang-format on
    EXPECT_TRUE(answers.empty());
    EXPECT_EQ(dialogs.getPendingCount(), 2U);

    dialogs.pump();
    EXPECT_EQ(answers, (std::vector<std::string>{"button 1", "folder /exports"}));
    EXPECT_EQ(dialogs.getPendingCount(), 0U);
}

TEST_F(DialogsTest, CancelsAndTimesOutDialogsAndClosesThem) {
    Dialogs dialogs(host, directory.getPath() / "dialogs");
    const std::uint64_t given = dialogs.show(message({"OK"}), record());
    const std::uint64_t slow = dialogs.show(message({"OK"}), record(), std::chrono::milliseconds(1));
    const std::uint64_t patient = dialogs.show(message({"OK"}), record(), std::chrono::hours(1));
    EXPECT_TRUE(dialogs.cancel(given));
    EXPECT_FALSE(dialogs.cancel(given));
    EXPECT_EQ(host.getCancelledDialogs(), (std::vector<std::uint64_t>{given}));

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    dialogs.resolve(given, {.button = 0});
    dialogs.pump();
    EXPECT_EQ(answers, (std::vector<std::string>{"cancelled: The dialog was cancelled.", "timeout: The dialog timed out."}));
    EXPECT_EQ(host.getCancelledDialogs(), (std::vector<std::uint64_t>{given, slow}));

    // An answer after the timeout is dropped, and the dialog that has time left still waits.
    dialogs.resolve(slow, {.button = 0});
    dialogs.pump();
    EXPECT_EQ(answers.size(), 2U);
    EXPECT_EQ(dialogs.getPendingCount(), 1U);
    dialogs.resolve(patient, {.failure = DialogResult::Failure{.code = DialogResult::Code::Unsupported, .message = "No dialogs here."}});
    dialogs.pump();
    EXPECT_EQ(answers.back(), "unsupported: No dialogs here.");
}

TEST_F(DialogsTest, EmptiesItsFolderAndClosesOpenDialogsWithTheEngine) {
    directory.write("dialogs/7/photo.png", "old copy");
    {
        Dialogs dialogs(host, directory.getPath() / "dialogs");
        EXPECT_FALSE(std::filesystem::exists(directory.getPath() / "dialogs"));
        const std::uint64_t open = dialogs.show(message({"OK"}), record());
        const std::uint64_t answered = dialogs.show(message({"OK"}), record());
        dialogs.resolve(answered, {.button = 0});
        dialogs.pump();
        EXPECT_EQ(host.getCancelledDialogs(), std::vector<std::uint64_t>{});
        dialogs.resolve(open, {});
    }
    ASSERT_EQ(host.getCancelledDialogs().size(), 1U);
    EXPECT_EQ(host.getCancelledDialogs().front(), host.getDialogRequests().front().id);
    EXPECT_EQ(answers, (std::vector<std::string>{"button 0"}));
}

TEST(DialogRelayTest, AnswersTheDialogsOfTheRunningAppOnly) {
    test::EngineFixture fixture;
    std::vector<std::string> answers;
    const std::uint64_t first = fixture.engine().getDialogs().show({.dialog = DialogRequest::OpenFolder{}}, [&answers](DialogResult result) { answers.push_back(result.folder.value_or("none")); });
    std::thread([first] { DialogRelay::resolve(first, {.folder = "/exports"}); }).join();
    fixture.frames(1);
    EXPECT_EQ(answers, (std::vector<std::string>{"/exports"}));

    // The platform answers a dialog of the app that stopped, which the new app never showed.
    const std::uint64_t stale = fixture.engine().getDialogs().show({.dialog = DialogRequest::OpenFolder{}}, [&answers](DialogResult) { answers.emplace_back("stale"); });
    fixture.restart();
    EXPECT_EQ(fixture.host().getCancelledDialogs(), (std::vector<std::uint64_t>{stale}));
    DialogRelay::resolve(stale, {.folder = "/late"});
    fixture.frames(1);
    EXPECT_EQ(answers.size(), 1U);
    EXPECT_EQ(fixture.engine().getDialogs().getPendingCount(), 0U);
}

TEST(DialogsLuaTest, ReturnsTheChoiceOfEveryDialog) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        dialogs = require('haylen.dialogs')
        async = require('async')
        results = {}
        local function keep(name, call)
            async.spawn(function()
                local value, err = call:await()
                results[name] = {value = value, err = err}
            end)
        end
        keep('message', dialogs.message({title = 'Island', text = 'Leave the island?', kind = 'warning', buttons = {'Stay', 'Leave'}}))
        keep('dismissed', dialogs.message({text = 'Saved.', buttons = {'OK'}}))
        keep('files', dialogs.openFiles({title = 'Import', filters = {{name = 'Maps', extensions = {'tmj', 'json'}}}, multiple = true}))
        keep('noFiles', dialogs.openFiles())
        keep('saved', dialogs.saveFile({name = 'slot.json', data = '{"gold":\0 3}', filters = {{name = 'Saves', extensions = {'json'}}}}))
        keep('folder', dialogs.openFolder({title = 'Export to'}))
    )");
    // clang-format on
    const std::vector<HeadlessHost::DialogCall>& requests = fixture.host().getDialogRequests();
    ASSERT_EQ(requests.size(), 6U);
    const auto& message = std::get<DialogRequest::Message>(requests[0].request.dialog);
    EXPECT_EQ(message.title, "Island");
    EXPECT_EQ(message.kind, DialogRequest::MessageKind::Warning);
    EXPECT_EQ(message.buttons, (std::vector<std::string>{"Stay", "Leave"}));
    EXPECT_EQ(std::get<DialogRequest::Message>(requests[1].request.dialog).kind, DialogRequest::MessageKind::Info);
    const auto& files = std::get<DialogRequest::OpenFiles>(requests[2].request.dialog);
    EXPECT_TRUE(files.multiple);
    ASSERT_EQ(files.filters.size(), 1U);
    EXPECT_EQ(files.filters[0].extensions, (std::vector<std::string>{"tmj", "json"}));
    EXPECT_FALSE(std::get<DialogRequest::OpenFiles>(requests[3].request.dialog).multiple);
    const auto& saved = std::get<DialogRequest::SaveFile>(requests[4].request.dialog);
    EXPECT_EQ(saved.name, "slot.json");
    EXPECT_EQ(std::string(saved.data.begin(), saved.data.end()), std::string("{\"gold\":\0 3}", 12));
    EXPECT_EQ(std::get<DialogRequest::OpenFolder>(requests[5].request.dialog).title, "Export to");

    Dialogs& dialogs = fixture.engine().getDialogs();
    dialogs.resolve(requests[0].id, {.button = 1});
    dialogs.resolve(requests[1].id, {});
    dialogs.resolve(requests[2].id, {.files = {{.name = "north.tmj", .path = "/maps/north.tmj"}, {.name = "south.json", .path = "/maps/south.json"}}});
    dialogs.resolve(requests[3].id, {});
    dialogs.resolve(requests[4].id, {.saved = DialogResult::File{.name = "slot.json"}});
    dialogs.resolve(requests[5].id, {.folder = "/exports"});
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return results.folder ~= nil") == "true"; }));

    EXPECT_EQ(fixture.lua("return results.message.value"), "2");
    EXPECT_EQ(fixture.lua("return tostring(results.dismissed.value) .. ' ' .. tostring(results.dismissed.err)"), "nil nil");
    EXPECT_EQ(fixture.lua("local list = results.files.value return #list .. ' ' .. list[1].name .. ' ' .. list[2].path"), "2 north.tmj /maps/south.json");
    EXPECT_EQ(fixture.lua("return tostring(results.noFiles.value)"), "nil");
    EXPECT_EQ(fixture.lua("return results.saved.value.name .. ' ' .. tostring(results.saved.value.path)"), "slot.json nil");
    EXPECT_EQ(fixture.lua("return results.folder.value"), "/exports");
}

TEST(DialogsLuaTest, FailsWithTypedErrorsCancelsAndTimesOut) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        dialogs = require('haylen.dialogs')
        async = require('async')
        unsupported = dialogs.openFolder()
        given = dialogs.message({text = 'Wait?', buttons = {'OK'}})
        slow = dialogs.message({text = 'Quick!', buttons = {'OK'}, timeout = 0.05})
        async.spawn(function()
            local _, missing = unsupported:await()
            local _, cancel = given:await()
            local _, late = slow:await()
            summary = table.concat({missing.code, missing.message, tostring(missing), cancel.code, cancel.message, late.code, late.message, tostring(given.done)}, '|')
        end)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return tostring(given.done) .. ' ' .. tostring(given.id ~= slow.id)"), "false true");
    fixture.engine().getDialogs().resolve(std::stoull(fixture.lua("return unsupported.id")), {.failure = DialogResult::Failure{.code = DialogResult::Code::Unsupported, .message = "Native dialogs are not implemented here."}});
    EXPECT_EQ(fixture.lua("return given:cancel()"), "true");
    EXPECT_EQ(fixture.lua("return given:cancel()"), "false");

    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return summary ~= nil") == "true"; }));
    EXPECT_EQ(fixture.lua("return summary"), "unsupported|Native dialogs are not implemented here.|Native dialogs are not implemented here.|cancelled|The dialog was cancelled.|timeout|The dialog timed out.|true");
    EXPECT_EQ(fixture.host().getCancelledDialogs(), (std::vector<std::uint64_t>{std::stoull(fixture.lua("return given.id")), std::stoull(fixture.lua("return slow.id"))}));

    // The promise of a call joins the combinators of `async`, which see only the message of a failure.
    fixture.runLua("both = dialogs.openFolder() async.spawn(function() local _, err = async.all({both.promise}):await() joined = tostring(err) end)");
    fixture.engine().getDialogs().resolve(std::stoull(fixture.lua("return both.id")), {.failure = DialogResult::Failure{.code = DialogResult::Code::Failed, .message = "The picker broke."}});
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return joined ~= nil") == "true"; }));
    EXPECT_NE(fixture.lua("return joined").find("The picker broke."), std::string::npos);
}

TEST(DialogsLuaTest, RaisesClearErrorsForInvalidOptions) {
    test::EngineFixture fixture;
    fixture.runLua("dialogs = require('haylen.dialogs')");
    // clang-format off
    const auto fails = [&fixture](const std::string& source, const std::string& expected) {
        const std::string error = fixture.lua(source);
        EXPECT_NE(error.find(expected), std::string::npos) << source << " -> " << error;
    };
    // clang-format on
    fails("dialogs.message({txt = 'Hi', buttons = {'OK'}})", "Unknown option \"txt\".");
    fails("dialogs.message({text = 'Hi', buttons = {'OK'}, kind = 'fatal'})", "The option \"kind\" of \"message\" is invalid: unknown value 'fatal'.");
    fails("dialogs.message({text = 'Hi', buttons = 'OK'})", "The option \"buttons\" of \"message\" is invalid");
    fails("dialogs.message({buttons = {'OK'}})", "A message dialog needs a text.");
    fails("dialogs.message({text = 'Hi'})", "A message dialog has one to three buttons, not 0.");
    fails("dialogs.message()", "table expected");
    fails("dialogs.openFiles({filters = 'png'})", "The filters of a file dialog are a list of tables with a name and a list of extensions.");
    fails("dialogs.openFiles({filters = {'png'}})", "The filters of a file dialog are a list of tables with a name and a list of extensions.");
    fails("dialogs.openFiles({filters = {{name = 'Images', extension = {'png'}}}})", "Unknown option \"extension\".");
    fails("dialogs.openFiles({filters = {{name = 'Images', extensions = {'.png'}}}})", "The extension \".png\" of the filter \"Images\" is invalid.");
    fails("dialogs.openFiles({multiple = 'yes'})", "The option \"multiple\" of \"openFiles\" is invalid");
    fails("dialogs.saveFile({name = 'slot.json'})", "A save dialog needs the data it writes, as a string.");
    fails("dialogs.saveFile({data = ''})", "A save dialog needs the name it suggests for the file.");
    fails("dialogs.openFolder({title = 'Export', timeout = 0})", "The timeout of a dialog is a positive number of seconds.");
    fails("dialogs.openFolder({title = 'Export', timeout = 'soon'})", "The option \"timeout\" of \"openFolder\" is invalid");
    EXPECT_TRUE(fixture.host().getDialogRequests().empty());
}

} // namespace haylen::platform
