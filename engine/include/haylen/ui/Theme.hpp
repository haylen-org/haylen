#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "haylen/2d/graphics/NineSlice.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"

struct ImGuiStyle;

namespace haylen::ui {

// Every semantic color, metric, font and surface a component may ask for, so no component reads a literal.
class Theme final {
  public:
    // Colors come in families: surfaces, states, borders, text, the accent and four tones. A tone has its fill, the ink written on that fill, a subtle background and a text color readable on the window.
    enum class Color : std::uint8_t {
        Window,
        Panel,
        Raised,
        Tooltip,
        Overlay,
        Hover,
        Pressed,
        Selection,
        Focus,
        Border,
        BorderStrong,
        Scrollbar,
        ScrollbarHover,
        Text,
        TextMuted,
        TextDisabled,
        OnTooltip,
        Accent,
        AccentHover,
        AccentStrong,
        OnAccent,
        AccentBackground,
        AccentText,
        Success,
        OnSuccess,
        SuccessBackground,
        SuccessText,
        Warning,
        OnWarning,
        WarningBackground,
        WarningText,
        Danger,
        DangerHover,
        DangerStrong,
        OnDanger,
        DangerBackground,
        DangerText,
        Information,
        OnInformation,
        InformationBackground,
        InformationText,
    };

    enum class Metric : std::uint8_t {
        ControlHeight,
        ControlRadius,
        ControlPaddingX,
        ControlPaddingY,
        ItemSpacing,
        PanelPadding,
        BorderWidth,
        FocusWidth,
        ScrollbarSize,
        IconSize,
        ChoiceSize,
        SliderTrackHeight,
        SliderKnobSize,
        ToggleWidth,
        ToggleHeight,
        ProgressHeight,
        BadgePaddingX,
        BadgePaddingY,
        TabPaddingX,
        ListRowHeight,
        DialogWidth,
        ToastWidth,
        TooltipWidth,
        SettingsLabelWidth,
        CaretWidth,
        CircularProgressSize,
        CircularProgressThickness,
        SlotSize,
        WindowTitleHeight,
        PageIndicatorSize,
    };

    enum class Font : std::uint8_t {
        Body,
        Caption,
        Button,
        Heading,
        Title,
        Monospace,
    };

    // Surfaces a theme may paint with images instead of flat colors, such as the wooden panels and ribbons of a textured game theme.
    enum class Surface : std::uint8_t {
        Panel,
        Card,
        Dialog,
        Tooltip,
        Toast,
        Banner,
        Button,
        ButtonHover,
        ButtonPressed,
        ButtonPrimary,
        ButtonPrimaryHover,
        ButtonPrimaryPressed,
        ButtonDestructive,
        ButtonDestructiveHover,
        ButtonDestructivePressed,
        Field,
        FieldFocused,
        Check,
        CheckChecked,
        Track,
        TrackFill,
        Knob,
        Tab,
        TabSelected,
        Chip,
        ChipSelected,
        Badge,
        StickBase,
        StickKnob,
        TouchButton,
        TouchButtonPressed,
        Segment,
        SegmentSelected,
        Menu,
        Window,
        Slot,
        SlotHighlighted,
    };

    // The font a role draws with, its size and the style of its face, where a font without a face for the style draws with its regular face.
    struct FontStyle {
        std::string font = "default";
        float size = 30.0F;
        bool bold = false;
        bool italic = false;
    };

    // A nine-slice image drawn in place of a flat surface. Borders keep their texture size times the scale, and the padding moves content away from thick borders.
    struct Image {
        graphics2d::NineSlice slice;
        float scale = 1.0F;
        math::Insets padding;
        math::Color tint = math::Color::white();
        bool colorize = false;
    };

    // Loads a texture for a theme image from a path relative to the package `content/` folder.
    using TextureLoader = std::function<graphics::Texture(std::string_view path, graphics::Texture::Options options)>;

    static constexpr std::size_t kColorCount = static_cast<std::size_t>(Color::InformationText) + 1;
    static constexpr std::size_t kMetricCount = static_cast<std::size_t>(Metric::PageIndicatorSize) + 1;
    static constexpr std::size_t kFontCount = static_cast<std::size_t>(Font::Monospace) + 1;
    static constexpr std::size_t kSurfaceCount = static_cast<std::size_t>(Surface::SlotHighlighted) + 1;

    [[nodiscard]] static Theme dark();
    [[nodiscard]] static Theme light();

    // Reads a theme on top of a base theme. The JSON holds `name`, optional `colors`, `metrics`, `fonts`, `fontFiles` and `surfaces`, and `fontFiles` maps font names to TrueType files the caller registers before drawing.
    [[nodiscard]] static Theme fromJson(const core::Json& document, const Theme& base, const TextureLoader& loadTexture);

    [[nodiscard]] static std::optional<Color> colorFromName(std::string_view value) noexcept;
    [[nodiscard]] static std::optional<Metric> metricFromName(std::string_view value) noexcept;
    [[nodiscard]] static std::optional<Font> fontFromName(std::string_view value) noexcept;
    [[nodiscard]] static std::optional<Surface> surfaceFromName(std::string_view value) noexcept;

    [[nodiscard]] const std::string& getName() const noexcept {
        return name;
    }
    [[nodiscard]] math::Color getColor(Color role) const noexcept {
        return colors[static_cast<std::size_t>(role)];
    }
    [[nodiscard]] float getMetric(Metric role) const noexcept {
        return metrics[static_cast<std::size_t>(role)];
    }
    [[nodiscard]] const FontStyle& getFont(Font role) const noexcept {
        return fonts[static_cast<std::size_t>(role)];
    }
    [[nodiscard]] const Image* getSurface(Surface role) const noexcept;
    [[nodiscard]] const std::map<std::string, std::string, std::less<>>& getFontFiles() const noexcept {
        return fontFiles;
    }

    void setColor(Color role, math::Color value) noexcept {
        colors[static_cast<std::size_t>(role)] = value;
    }
    void setMetric(Metric role, float value);
    void setFont(Font role, FontStyle value);
    void setSurface(Surface role, std::optional<Image> value);

    // Copies the colors and shapes into an ImGui style, so immediate windows match the retained components.
    void applyTo(ImGuiStyle& style) const;

  private:
    using Palette = std::array<std::uint32_t, kColorCount>;

    static constexpr std::array<std::string_view, kColorCount> kColorNames{
        "window", "panel", "raised", "tooltip", "overlay", "hover", "pressed", "selection", "focus", "border", "borderStrong", "scrollbar", "scrollbarHover", "text", "textMuted", "textDisabled", "onTooltip", "accent", "accentHover", "accentStrong", "onAccent", "accentBackground", "accentText", "success", "onSuccess", "successBackground", "successText", "warning", "onWarning", "warningBackground", "warningText", "danger", "dangerHover", "dangerStrong", "onDanger", "dangerBackground", "dangerText", "information", "onInformation", "informationBackground", "informationText",
    };
    static constexpr std::array<std::string_view, kMetricCount> kMetricNames{
        "controlHeight", "controlRadius", "controlPaddingX", "controlPaddingY", "itemSpacing", "panelPadding", "borderWidth", "focusWidth", "scrollbarSize", "iconSize", "choiceSize", "sliderTrackHeight", "sliderKnobSize", "toggleWidth", "toggleHeight", "progressHeight", "badgePaddingX", "badgePaddingY", "tabPaddingX", "listRowHeight", "dialogWidth", "toastWidth", "tooltipWidth", "settingsLabelWidth", "caretWidth", "circularProgressSize", "circularProgressThickness", "slotSize", "windowTitleHeight", "pageIndicatorSize",
    };
    static constexpr std::array<std::string_view, kFontCount> kFontNames{"body", "caption", "button", "heading", "title", "monospace"};
    static constexpr std::array<std::string_view, kSurfaceCount> kSurfaceNames{
        "panel", "card", "dialog", "tooltip", "toast", "banner", "button", "buttonHover", "buttonPressed", "buttonPrimary", "buttonPrimaryHover", "buttonPrimaryPressed", "buttonDestructive", "buttonDestructiveHover", "buttonDestructivePressed", "field", "fieldFocused", "check", "checkChecked", "track", "trackFill", "knob", "tab", "tabSelected", "chip", "chipSelected", "badge", "stickBase", "stickKnob", "touchButton", "touchButtonPressed", "segment", "segmentSelected", "menu", "window", "slot", "slotHighlighted",
    };
    static constexpr Palette kDarkPalette{
        0x1B1E2BFF, 0x232739FF, 0x2C3147FF, 0x0F111AF2, 0x000000A0, 0xFFFFFF14, 0xFFFFFF24, 0x4C7DFF40, 0x7AA2FFFF, 0x3A4058FF, 0x525A7AFF, 0x3A4058FF, 0x525A7AFF, 0xE8EAF2FF, 0xA3A8BFFF, 0x6A7090FF, 0xE8EAF2FF, 0x4C7DFFFF, 0x6690FFFF, 0x3A66E0FF, 0xFFFFFFFF, 0x4C7DFF26, 0x8FB0FFFF, 0x3DBE7AFF, 0xFFFFFFFF, 0x3DBE7A26, 0x6FDCA0FF, 0xF2B23AFF, 0x1B1E2BFF, 0xF2B23A26, 0xF7CB70FF, 0xE5534BFF, 0xEE6B64FF, 0xC8423BFF, 0xFFFFFFFF, 0xE5534B26, 0xFF8A84FF, 0x3AA8E0FF, 0xFFFFFFFF, 0x3AA8E026, 0x7FCBF2FF,
    };
    static constexpr Palette kLightPalette{
        0xF4F5F9FF, 0xFFFFFFFF, 0xFFFFFFFF, 0x1B1E2BF2, 0x00000066, 0x0000000F, 0x0000001F, 0x4C7DFF33, 0x3A66E0FF, 0xD6D9E4FF, 0xB3B8CCFF, 0xC9CDDBFF, 0xA9AEC2FF, 0x1B1E2BFF, 0x5C6380FF, 0xA3A8BFFF, 0xF4F5F9FF, 0x3A66E0FF, 0x4C7DFFFF, 0x2C52C0FF, 0xFFFFFFFF, 0x3A66E01F, 0x2C52C0FF, 0x2E9E62FF, 0xFFFFFFFF, 0x2E9E621F, 0x1F7A49FF, 0xD9941CFF, 0xFFFFFFFF, 0xD9941C1F, 0x9A6508FF, 0xD0433BFF, 0xE5534BFF, 0xB0352EFF, 0xFFFFFFFF, 0xD0433B1F, 0xA8322BFF, 0x2A8CC0FF, 0xFFFFFFFF, 0x2A8CC01F, 0x1D6A93FF,
    };

    // Sizes suit the 1920 by 1080 design resolution the engine uses by default.
    static constexpr std::array<float, kMetricCount> kMetrics{64.0F, 12.0F, 24.0F, 12.0F, 16.0F, 28.0F, 2.0F, 3.0F, 16.0F, 36.0F, 36.0F, 10.0F, 34.0F, 72.0F, 38.0F, 22.0F, 14.0F, 4.0F, 24.0F, 64.0F, 760.0F, 560.0F, 520.0F, 420.0F, 2.0F, 72.0F, 8.0F, 96.0F, 56.0F, 14.0F};
    static constexpr std::array<float, kFontCount> kFontSizes{30.0F, 24.0F, 30.0F, 38.0F, 56.0F, 26.0F};

    // The edges and the center of a nine-slice, the pieces a tiled image repeats, and the smallest size a copy of them may have.
    static constexpr std::array<std::size_t, 5> kTiledPieces{1, 3, 4, 5, 7};
    static constexpr float kMinTileSize = 1.0F;

    template <typename Enum, std::size_t Count> [[nodiscard]] static std::optional<Enum> fromName(const std::array<std::string_view, Count>& names, std::string_view value) noexcept;
    [[nodiscard]] static math::Color readColor(const core::Json& value, const std::string& context);
    [[nodiscard]] static float readNumber(const core::Json& value, const std::string& context);
    [[nodiscard]] static math::Insets readInsets(const core::Json& value, const std::string& context);
    [[nodiscard]] static math::Rect readRect(const core::Json& value, const std::string& context);
    [[nodiscard]] static Image readImage(const core::Json& value, const std::string& context, const TextureLoader& loadTexture);
    static void checkTiles(const Image& image, const std::string& context);
    [[nodiscard]] static const core::Json& readSection(const core::Json& document, const char* key);

    std::string name;
    std::array<math::Color, kColorCount> colors{};
    std::array<float, kMetricCount> metrics{};
    std::array<FontStyle, kFontCount> fonts{};
    std::array<std::optional<Image>, kSurfaceCount> surfaces{};
    std::map<std::string, std::string, std::less<>> fontFiles;
};

} // namespace haylen::ui
