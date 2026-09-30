#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "support/EngineFixture.hpp"
#include "ui/TextSession.hpp"

namespace haylen::ui {

namespace {

// Drives text fields as a native field of the platform would, through the recording text input of the headless host.
class TextInputTest : public ::testing::Test {
  protected:
    using TextInput = platform::TextInput;

    TextInputTest() {
        getInput().setNative(true);
        // clang-format off
        connection = getUi().events.connect([this](Document&, const Event& event) {
            if (event.name != "focus" && event.name != "blur") {
                events.push_back(event.id + ":" + event.name + " " + event.value.dump());
            }
        });
        // clang-format on
    }

    core::Engine& getEngine() {
        return fixture.engine();
    }
    plugins::UiPlugin& getUi() {
        return getEngine().getPlugin<plugins::UiPlugin>();
    }
    platform::HeadlessTextInput& getInput() {
        return fixture.host().getTextInput();
    }
    const TextInput::Field& getPublished() {
        return getInput().getPublished().back();
    }

    std::shared_ptr<Document> mount(const std::string& json) {
        auto document = getUi().createDocument(core::Json::parse(json), Placement::Screen);
        getUi().mount(document);
        fixture.frames(1);
        return document;
    }

    void focus(Document& document, const std::string& id) {
        document.command(getUi().getContext(), id, "focus", core::Json::object());
        fixture.frames(2);
    }

    void send(platform::Event event) {
        getEngine().handleEvent(event);
        fixture.frames(2);
    }

    void edit(std::string text, int selectionStart, int selectionEnd, int compositionStart = -1, int compositionEnd = -1) {
        const TextInput::Field& field = getPublished();
        send({.type = platform::Event::Type::TextEdited, .textEdit = {.field = field.id, .revision = field.revision, .text = std::move(text), .selectionStart = selectionStart, .selectionEnd = selectionEnd, .compositionStart = compositionStart, .compositionEnd = compositionEnd}});
    }

    void act(TextInput::Action action) {
        send({.type = platform::Event::Type::TextAction, .textEdit = {.field = getPublished().id}, .textAction = action});
    }

    // Presses a key the way a physical keyboard does, with the character it types in the same frame as the key.
    void press(input::Key key, char32_t character, input::KeyModifiers modifiers = {}) {
        getEngine().handleEvent({.type = platform::Event::Type::KeyDown, .key = key, .modifiers = modifiers});
        if (character != 0) {
            getEngine().handleEvent({.type = platform::Event::Type::Character, .modifiers = modifiers, .character = character});
        }
        fixture.frames(1);
        getEngine().handleEvent({.type = platform::Event::Type::KeyUp, .key = key, .modifiers = modifiers});
        fixture.frames(1);
    }

    test::EngineFixture fixture;
    std::vector<std::string> events;
    core::Connection connection;
};

} // namespace

TEST_F(TextInputTest, PublishesTheFocusedFieldWithItsKeyboard) {
    // clang-format off
    auto document = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "textField", "id": "email", "value": "ana", "keyboard": "email", "returnKey": "go", "autocorrect": true, "autocapitalize": "words", "maxLength": 40},
        {"kind": "textArea", "id": "notes"},
        {"kind": "secretField", "id": "pin"},
        {"kind": "numberField", "id": "count", "min": -5, "max": 5}
    ]})");
    // clang-format on

    // Every field on screen is offered to the platform with its keyboard, and the defaults follow the keyboard.
    const std::vector<TextInput::Field>& visible = getInput().getVisibleFields();
    ASSERT_EQ(visible.size(), 4U);
    EXPECT_EQ(visible[0].bounds, document->find("email")->getBounds());
    EXPECT_EQ(visible[0].options, (TextInput::Options{.keyboard = TextInput::Keyboard::Email, .returnKey = TextInput::ReturnKey::Go, .capitalization = TextInput::Capitalization::Words, .autocorrect = true, .maxLength = 40}));
    EXPECT_EQ(visible[1].options, (TextInput::Options{.keyboard = TextInput::Keyboard::Multiline}));
    EXPECT_EQ(visible[2].options, (TextInput::Options{.keyboard = TextInput::Keyboard::Password, .capitalization = TextInput::Capitalization::None, .autocorrect = false}));
    EXPECT_EQ(visible[3].options.keyboard, TextInput::Keyboard::Text) << "Numeric keypads have no minus sign.";
    EXPECT_TRUE(getInput().getPublished().empty());

    focus(*document, "email");
    ASSERT_TRUE(getInput().isEditing());
    const TextInput::Field& field = getPublished();
    EXPECT_EQ(field.id, visible[0].id);
    EXPECT_EQ(field.text, "ana");
    EXPECT_EQ(field.selectionStart, 0) << "A field focused from the keyboard selects its text.";
    EXPECT_EQ(field.selectionEnd, 3);
    EXPECT_EQ(field.bounds, visible[0].bounds);
    EXPECT_FALSE(field.caret.isEmpty());
    EXPECT_TRUE(field.bounds.contains(field.caret.getCenter()));
    EXPECT_FALSE(fixture.host().isKeyboardVisible()) << "Fields of the UI never open the plain keyboard.";

    getUi().getContext().getBackend().makeCurrent();
    ImGui::ClearActiveID();
    fixture.frames(1);
    EXPECT_FALSE(getInput().isEditing());
}

TEST_F(TextInputTest, AppliesEditsOfTheCurrentRevisionOnly) {
    auto document = mount(R"({"kind": "textField", "id": "name", "value": "Ana"})");
    focus(*document, "name");
    const std::uint64_t revision = getPublished().revision;
    const std::uint64_t id = getPublished().id;
    events.clear();

    edit("Ana é", 5, 5);
    EXPECT_EQ(events, std::vector<std::string>{R"(name:change {"value":"Ana é"})"});
    EXPECT_EQ(getPublished().revision, revision) << "An edit of the platform is no change of the UI.";
    EXPECT_EQ(getPublished().text, "Ana é");
    EXPECT_EQ(getPublished().selectionStart, 5) << "Selections count code points.";

    // An edit against an older revision or another field answers a text the field no longer shows.
    send({.type = platform::Event::Type::TextEdited, .textEdit = {.field = id, .revision = revision - 1, .text = "stale"}});
    send({.type = platform::Event::Type::TextEdited, .textEdit = {.field = id + 1, .revision = revision, .text = "other"}});
    EXPECT_EQ(events.size(), 1U);

    edit("Ana é", 0, 3);
    EXPECT_EQ(events.size(), 1U);
    EXPECT_EQ(getPublished().selectionStart, 0);
    EXPECT_EQ(getPublished().selectionEnd, 3);
    EXPECT_EQ(getPublished().revision, revision);
}

TEST_F(TextInputTest, WaitsForTheInputMethodToCommit) {
    auto document = mount(R"({"kind": "textField", "id": "name", "maxLength": 3})");
    focus(*document, "name");
    const std::size_t published = getInput().getPublished().size();
    const ImGuiID field = static_cast<ImGuiID>(getPublished().id);
    const TextSession& session = getUi().getBackend().getTextSession();

    edit("かな", 2, 2, 0, 2);
    EXPECT_EQ(session.getComposition(field), (std::pair{0, 2}));
    EXPECT_EQ(getInput().getPublished().size(), published) << "Nothing reaches the platform while it composes.";

    edit("仮名", 2, 2);
    EXPECT_FALSE(session.getComposition(field));
    EXPECT_EQ(getPublished().text, "仮名");

    // The limit cuts what the platform typed, which the UI then pushes back as a change of its own.
    const std::uint64_t revision = getPublished().revision;
    edit("仮名です", 4, 4);
    EXPECT_EQ(getPublished().text, "仮名で");
    EXPECT_GT(getPublished().revision, revision);
    EXPECT_EQ(getPublished().selectionEnd, 3);
}

TEST_F(TextInputTest, LeavesTypingToTheNativeField) {
    auto document = mount(R"({"kind": "textField", "id": "name", "value": "abc"})");
    focus(*document, "name");
    events.clear();

    send({.type = platform::Event::Type::Character, .character = U'x'});
    send({.type = platform::Event::Type::KeyDown, .key = input::Key::Backspace});
    send({.type = platform::Event::Type::KeyUp, .key = input::Key::Backspace});
    EXPECT_TRUE(events.empty());
    EXPECT_EQ(getPublished().text, "abc");
}

// Where the UI edits the text itself, Space and Enter type into the field even though they also press the focused control.
TEST_F(TextInputTest, TypesTheKeysThatPressControls) {
    getInput().setNative(false);
    auto document = mount(R"({"kind": "column", "children": [{"kind": "textField", "id": "name"}, {"kind": "textArea", "id": "notes"}]})");

    focus(*document, "name");
    events.clear();
    press(input::Key::A, U'a');
    press(input::Key::Space, U' ');
    press(input::Key::B, U'b');
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.back(), R"(name:change {"value":"a b"})");
    press(input::Key::Enter, U'\r');
    EXPECT_EQ(events.back(), R"(name:submit {"value":"a b"})");

    focus(*document, "notes");
    events.clear();
    press(input::Key::A, U'a');
    press(input::Key::Enter, U'\r');
    press(input::Key::B, U'b');
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.back(), R"(notes:change {"value":"a\nb"})");
}

// A shift press selects from the caret where it stands, also after typing moved it.
TEST_F(TextInputTest, SelectsFromTheCaretAfterTyping) {
    getInput().setNative(false);
    auto document = mount(R"({"kind": "textField", "id": "name"})");
    focus(*document, "name");
    press(input::Key::A, U'a');
    press(input::Key::B, U'b');
    press(input::Key::C, U'c');
    press(input::Key::Left, 0, {.shift = true});
    EXPECT_EQ(getPublished().text, "abc");
    EXPECT_EQ(getPublished().selectionStart, 2);
    EXPECT_EQ(getPublished().selectionEnd, 3);
}

TEST_F(TextInputTest, TurnsActionsIntoSubmitNextCancelAndDismiss) {
    // clang-format off
    auto document = mount(R"({"kind": "column", "children": [
        {"kind": "textField", "id": "first", "value": "a"},
        {"kind": "textField", "id": "second", "returnKey": "next"},
        {"kind": "textField", "id": "third"}
    ]})");
    // clang-format on
    const std::vector<TextInput::Field> fields = getInput().getVisibleFields();
    ASSERT_EQ(fields.size(), 3U);

    focus(*document, "first");
    act(TextInput::Action::Submit);
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.back(), R"(first:submit {"value":"a"})");
    EXPECT_FALSE(getInput().isEditing());

    focus(*document, "first");
    act(TextInput::Action::Next);
    EXPECT_EQ(getPublished().id, fields[1].id);

    // A return key labelled `next` moves on like tab.
    events.clear();
    act(TextInput::Action::Submit);
    EXPECT_EQ(getPublished().id, fields[2].id);
    EXPECT_TRUE(events.empty());

    // Cancel reverts what the field took since it was focused.
    edit("typo", 4, 4);
    act(TextInput::Action::Cancel);
    EXPECT_FALSE(getInput().isEditing());
    EXPECT_EQ(events.back(), R"(third:change {"value":""})");

    focus(*document, "first");
    events.clear();
    act(TextInput::Action::Dismissed);
    EXPECT_FALSE(getInput().isEditing());
    EXPECT_TRUE(events.empty());
}

TEST_F(TextInputTest, LiftsTheFocusedFieldAboveTheKeyboard) {
    auto document = mount(R"({"kind": "column", "children": [{"kind": "spacer", "height": 900}, {"kind": "textField", "id": "name"}]})");
    const math::Rect resting = document->find("name")->getBounds();
    focus(*document, "name");

    send({.type = platform::Event::Type::KeyboardChanged, .keyboardFrame = {0.0F, 540.0F, 1920.0F, 540.0F}});
    fixture.frames(60);
    const math::Rect lifted = document->find("name")->getBounds();
    EXPECT_LE(lifted.getBottom(), 540.0F);
    EXPECT_GE(lifted.getBottom(), 500.0F);
    EXPECT_EQ(getPublished().bounds, lifted) << "The platform follows the field.";

    send({.type = platform::Event::Type::KeyboardChanged});
    fixture.frames(60);
    EXPECT_EQ(document->find("name")->getBounds(), resting);
}

} // namespace haylen::ui
