#pragma once

#include <jni.h>

#include <cstdint>

#include "haylen/platform/TextInput.hpp"

namespace haylen::platform {

// Text input of Android. The hidden HaylenEditText of the activity edits the focused field over its place on screen, which brings the software keyboard, the composing text of input methods, suggestions and keyboard actions. Its UI thread reports back through JNI.
class AndroidTextInput final : public TextInput {
  public:
    [[nodiscard]] bool isNative() const noexcept override {
        return true;
    }
    void edit(const Field& value) override;
    void finish() override;

    // Whether the hidden field edits a text field of the UI, which then takes the keys of hardware keyboards itself.
    [[nodiscard]] static bool isEditing() noexcept {
        return editing;
    }

    // Lets the field being edited go, as the back key does, and returns whether a field was being edited.
    static bool dismiss();

    // Entries of the Java side, which reach the running app on its next frame. Positions arrive in window pixels.
    static void receiveEdit(JNIEnv& env, jlong field, jlong revision, jbyteArray text, jint selectionStart, jint selectionEnd, jint compositionStart, jint compositionEnd);
    static void receiveAction(jlong field, jint action);
    static void receiveKeyboard(jint x, jint y, jint width, jint height);

  private:
    static bool editing;
    static std::uint64_t editedField;
};

} // namespace haylen::platform
