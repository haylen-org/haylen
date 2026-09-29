#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/input/Controls.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"

namespace haylen::ui {

// A field that shows an action map binding and, once pressed, listens for the next key, mouse button, gamepad button or gamepad axis and takes it as its binding, such as key:w or button:south, for a controls settings screen.
class KeyCapture final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "keyCapture";
    }

    // Writes a binding for people, such as Left Shift for key:left_shift.
    [[nodiscard]] static std::string describe(std::string_view binding);

  protected:
    [[nodiscard]] bool isFocusable() const noexcept override {
        return true;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;
    void drawingStopped(Context&) override;

  private:
    enum class Source : std::uint8_t {
        Key,
        Mouse,
        Button,
        Axis,
    };

    static constexpr std::array<std::string_view, 4> kSourceNames{"key", "mouse", "button", "axis"};
    static constexpr float kAxisTravel = 0.6F;

    [[nodiscard]] static std::vector<std::string> readBindings(PropertyReader& reader, std::string_view key, const core::Json& listed);
    [[nodiscard]] std::optional<std::string> listen(const input::Input& devices) const;
    [[nodiscard]] bool accepts(Source source) const;
    void start(const input::Input& devices, std::uint64_t frame);
    void finish(Context& context, const std::string& binding);

    std::string value;
    TextValue placeholder;
    TextValue prompt;
    std::vector<std::string> sources;
    std::vector<std::string> cancelWith{"key:escape"};
    std::array<std::array<float, input::Controls::kGamepadAxisCount>, input::Input::kMaxGamepads> resting{};
    std::uint64_t startedFrame = 0;
    bool capturing = false;
};

} // namespace haylen::ui
