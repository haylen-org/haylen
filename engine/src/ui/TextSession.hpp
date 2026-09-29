#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "haylen/graphics/Viewport.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/TextInput.hpp"

namespace haylen::ui {

// Keeps the focused text field of the UI in step with the native field of the platform. The UI publishes the field with a revision that grows only when the UI itself changes the text or the selection, and applies an edit of the native field inside the ImGui callback of the field when the edit answers the current revision. Nothing is published while the input method composes, so a composition is never interrupted. Rectangles are in UI coordinates unless they cross to the platform, which works in framebuffer pixels.
class TextSession final {
  public:
    explicit TextSession(platform::TextInput& platformInput) : input(platformInput) {}

    void receiveEdit(platform::TextInput::Edit edit);

    // Keeps a submit, cancel or dismiss of the platform for the focused field until its editor applies it.
    void receiveAction(platform::TextInput::Action value) noexcept {
        action = value;
    }
    void setKeyboardFrame(const math::Rect& frame) noexcept {
        keyboard = frame;
    }

    // Starts a frame over the visible area, whose origin is the origin of UI coordinates, and moves the UI toward the offset that keeps the focused field above the keyboard.
    void beginFrame(const graphics::Viewport& frameViewport, float deltaSeconds);

    // Declares a text field drawn this frame before its InputText call, whose callback synchronizes it.
    void addField(ImGuiID id, const math::Rect& bounds, const platform::TextInput::Options& options);

    // Applies an edit of the native field and publishes what ImGui changed, called from the ImGui callback of the focused field.
    void synchronize(ImGuiInputTextCallbackData& data);

    // Records where the focused field shows its caret after its InputText call.
    void setCaret(const math::Rect& caret) noexcept {
        caretRect = caret;
    }

    // Publishes the focused field and the fields on screen to the platform at the end of the frame, or ends editing when no field kept the focus.
    void publish();

    [[nodiscard]] bool isActive() const noexcept {
        return activeThisFrame;
    }
    [[nodiscard]] bool isNativeEditing() const noexcept {
        return active != 0 && input.isNative();
    }
    [[nodiscard]] ImGuiID getActiveField() const noexcept {
        return active;
    }
    [[nodiscard]] const platform::TextInput::Options& getOptions() const noexcept {
        return options;
    }

    // Hands the editor of a field the action waiting for it. A cancel already brought back the text the field had when it took the focus.
    [[nodiscard]] std::optional<platform::TextInput::Action> takeAction(ImGuiID id);

    // Returns the code point range the input method composes in the field, or nothing when it composes nothing there.
    [[nodiscard]] std::optional<std::pair<int, int>> getComposition(ImGuiID id) const;

    // Returns the code point where the caret of the focused field stands.
    [[nodiscard]] int getCaret() const noexcept {
        return selectionEnd;
    }

    // How far up the UI moves so the focused field stays above the on-screen keyboard.
    [[nodiscard]] float getKeyboardOffset() const noexcept {
        return keyboardOffset;
    }

  private:
    struct Entry {
        ImGuiID id = 0;
        math::Rect bounds;
        platform::TextInput::Options options;
    };

    static constexpr float kKeyboardMargin = 16.0F;
    static constexpr float kKeyboardSpeed = 14.0F;

    // Every session of the process draws its revisions from one sequence, so an edit made for an app that restarted never matches the field of the new one.
    static std::uint64_t lastRevision;

    // Cuts the text to the length limit in code points and returns whether it cut anything.
    [[nodiscard]] static bool limit(ImGuiInputTextCallbackData& data, int maxLength);
    static void apply(ImGuiInputTextCallbackData& data, const platform::TextInput::Edit& edit);
    [[nodiscard]] static std::pair<int, int> getSelection(const ImGuiInputTextCallbackData& data);

    [[nodiscard]] math::Rect toFramebuffer(const math::Rect& rect) const noexcept;
    void publishVisibleFields();

    platform::TextInput& input;
    graphics::Viewport viewport;
    math::Vec2 origin;
    std::vector<Entry> fields;
    std::vector<platform::TextInput::Field> sentFields;
    std::optional<platform::TextInput::Field> published;
    std::optional<platform::TextInput::Edit> pending;
    std::optional<platform::TextInput::Action> action;
    ImGuiID active = 0;
    bool activeThisFrame = false;
    std::uint64_t revision = 0;
    std::string text;
    std::string original;
    int selectionStart = 0;
    int selectionEnd = 0;
    int compositionStart = -1;
    int compositionEnd = -1;
    math::Rect bounds;
    math::Rect caretRect;
    platform::TextInput::Options options;
    math::Rect keyboard;
    float keyboardOffset = 0.0F;
};

} // namespace haylen::ui
