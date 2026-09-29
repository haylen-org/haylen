#pragma once

#include "haylen/platform/TextInput.hpp"

namespace haylen::platform {

// Text input of Windows, which types through WM_CHAR and places the composition and candidate windows of the input method at the caret of the focused field.
class WindowsTextInput final : public TextInput {
  public:
    void edit(const Field& field) override;
};

} // namespace haylen::platform
