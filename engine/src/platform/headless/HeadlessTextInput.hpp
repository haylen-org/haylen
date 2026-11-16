#pragma once

#include <span>
#include <vector>

#include "haylen/platform/TextInput.hpp"

namespace haylen::platform {

// Text input of the headless host. It records what the UI publishes, so tests answer with edits and actions as a native field would.
class HeadlessTextInput final : public TextInput {
  public:
    [[nodiscard]] bool isNative() const noexcept override {
        return native;
    }
    void edit(const Field& field) override {
        published.push_back(field);
        editing = true;
    }
    void finish() override {
        editing = false;
    }
    void setVisibleFields(std::span<const Field> fields, float value) override {
        visibleFields.assign(fields.begin(), fields.end());
        tapSlop = value;
    }

    void setNative(bool value) noexcept {
        native = value;
    }
    [[nodiscard]] bool isEditing() const noexcept {
        return editing;
    }
    [[nodiscard]] const std::vector<Field>& getPublished() const noexcept {
        return published;
    }
    [[nodiscard]] const std::vector<Field>& getVisibleFields() const noexcept {
        return visibleFields;
    }
    [[nodiscard]] float getTapSlop() const noexcept {
        return tapSlop;
    }

  private:
    std::vector<Field> published;
    std::vector<Field> visibleFields;
    float tapSlop = 0.0F;
    bool native = false;
    bool editing = false;
};

} // namespace haylen::platform
