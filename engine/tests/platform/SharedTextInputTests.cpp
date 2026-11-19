#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>

#include "haylen/platform/TextInput.hpp"
#include "platform/SharedTextInput.hpp"
#include "platform/headless/HeadlessTextInput.hpp"

namespace haylen::platform {

class SharedTextInputTest : public ::testing::Test {
  protected:
    [[nodiscard]] static TextInput::Field makeField(std::uint64_t id) {
        return {.id = id, .revision = 1, .text = "Ana"};
    }

    // The field that the platform shows last.
    [[nodiscard]] const TextInput::Field& getShown() const {
        if (platformInput.getPublished().empty()) {
            throw std::runtime_error("The platform shows no field.");
        }

        return platformInput.getPublished().back();
    }

    HeadlessTextInput platformInput;
    SharedTextInput input{platformInput};
};

// A field that finishes hands the input back to the plain keyboard that the app keeps open, which starts empty.
TEST_F(SharedTextInputTest, GivesTheInputBackToThePlainKeyboardOfTheAppWhenAFieldFinishes) {
    input.setKeyboardVisible(true);
    ASSERT_TRUE(platformInput.isEditing());
    const std::uint64_t opened = getShown().revision;

    input.edit(makeField(7));
    EXPECT_EQ(getShown().id, 7U);

    input.finish();
    EXPECT_TRUE(platformInput.isEditing());
    EXPECT_EQ(getShown().id, TextInput::kKeyboardField);
    EXPECT_GT(getShown().revision, opened);
    EXPECT_TRUE(getShown().text.empty());
}

// The plain keyboard of the app waits while a field edits, and closing it leaves the field editing until it finishes.
TEST_F(SharedTextInputTest, KeepsAFieldEditingWhateverTheAppAsksOfThePlainKeyboard) {
    input.edit(makeField(7));
    input.setKeyboardVisible(true);
    EXPECT_EQ(getShown().id, 7U);

    input.setKeyboardVisible(false);
    EXPECT_TRUE(platformInput.isEditing());
    EXPECT_EQ(getShown().id, 7U);

    input.finish();
    EXPECT_FALSE(platformInput.isEditing());
}

// The plain keyboard that an editor holds, such as the one of the ImGui inputs of the UI, stays when the app closes its own, and asking for the keyboard again opens a new one.
TEST_F(SharedTextInputTest, KeepsThePlainKeyboardOfAnEditorAndOpensANewOneWhenAskedAgain) {
    input.setKeyboardVisible(true);
    const std::uint64_t opened = getShown().revision;
    input.edit(TextInput::getKeyboardField());
    EXPECT_EQ(getShown().revision, opened) << "The keyboard that already shows goes on.";

    input.setKeyboardVisible(false);
    EXPECT_TRUE(platformInput.isEditing());
    input.finish();
    EXPECT_FALSE(platformInput.isEditing());

    input.setKeyboardVisible(true);
    const std::uint64_t reopened = getShown().revision;
    input.setKeyboardVisible(true);
    EXPECT_GT(getShown().revision, reopened) << "A keyboard that the player closed shows again.";
}

} // namespace haylen::platform
