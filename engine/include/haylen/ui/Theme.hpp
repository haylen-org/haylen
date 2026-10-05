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
        Shadow,
        Hover,
        Pressed,
        Selection,
        Focus,
        Caret,
        Border,
        BorderStrong,
        Scrollbar,
        ScrollbarHover,
        Track,
        Knob,
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
        CellRadius,
        RefreshDistance,
        ToggleKnobInset,
        TransitionDuration,
        ToastLimit,
        ToneBarWidth,
        PanelRadius,
        ContentSpacing,
        FocusGap,
        DisabledOpacity,
        TooltipDelay,
        LongPressDuration,
        ShadowSize,
        ShadowOffset,
        ChipHeight,
        StrokeWidth,
        RowPadding,
        CheckRadius,
        MenuPadding,
        SplitterSize,
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
        Cell,
        CellHover,
        CellPressed,
        CellSelected,
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
    static constexpr std::size_t kMetricCount = static_cast<std::size_t>(Metric::SplitterSize) + 1;
    static constexpr std::size_t kFontCount = static_cast<std::size_t>(Font::Monospace) + 1;
    static constexpr std::size_t kSurfaceCount = static_cast<std::size_t>(Surface::CellSelected) + 1;

    [[nodiscard]] static Theme dark();
    [[nodiscard]] static Theme light();

    // Reads a theme on top of a base theme. The JSON holds `name`, optional `colors`, `metrics`, `fonts`, `fontFiles`, `surfaces` and `imageFilter`, and `fontFiles` maps font names to TrueType files the caller registers before drawing.
    [[nodiscard]] static Theme fromJson(const core::Json& document, const Theme& base, const TextureLoader& loadTexture);

    // Read the values of theme files and styles, where the context starts the message of an error, such as `The theme color "accent"`.
    [[nodiscard]] static math::Color readColor(const core::Json& value, const std::string& context);
    [[nodiscard]] static float readNumber(const core::Json& value, const std::string& context);
    [[nodiscard]] static Image readImage(const core::Json& value, const std::string& context, const TextureLoader& loadTexture);

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
    // The filter that the pictures of components, such as images, icons and image buttons, load with.
    [[nodiscard]] graphics::Texture::Filter getImageFilter() const noexcept {
        return imageFilter;
    }

    void setColor(Color role, math::Color value) noexcept {
        colors[static_cast<std::size_t>(role)] = value;
    }
    void setMetric(Metric role, float value);
    void setFont(Font role, FontStyle value);
    void setSurface(Surface role, std::optional<Image> value);
    void setImageFilter(graphics::Texture::Filter value) noexcept {
        imageFilter = value;
    }

    // Copies the colors and shapes into an ImGui style, so immediate windows match the retained components.
    void applyTo(ImGuiStyle& style) const;

  private:
    // A color role with its name and its colors in the dark and light themes, as `0xRRGGBBAA`.
    struct ColorRole {
        std::string_view name;
        std::uint32_t dark = 0;
        std::uint32_t light = 0;
    };

    // A metric with its name and its value in the built-in themes, which suits the 1920 by 1080 design resolution the engine uses by default.
    struct MetricRole {
        std::string_view name;
        float value = 0.0F;
    };

    // clang-format off
    static constexpr std::array<ColorRole, kColorCount> kColorRoles{{
        {"window", 0x1B1E2BFF, 0xF4F5F9FF},
        {"panel", 0x232739FF, 0xFFFFFFFF},
        {"raised", 0x2C3147FF, 0xFFFFFFFF},
        {"tooltip", 0x0F111AF2, 0x1B1E2BF2},
        {"overlay", 0x000000A0, 0x00000066},
        {"shadow", 0x00000066, 0x1B1E2B29},
        {"hover", 0xFFFFFF14, 0x0000000F},
        {"pressed", 0xFFFFFF24, 0x0000001F},
        {"selection", 0x4C7DFF40, 0x4C7DFF33},
        {"focus", 0x7AA2FFFF, 0x3A66E0FF},
        {"caret", 0x7AA2FFFF, 0x3A66E0FF},
        {"border", 0x3A4058FF, 0xD6D9E4FF},
        {"borderStrong", 0x525A7AFF, 0xB3B8CCFF},
        {"scrollbar", 0x3A4058FF, 0xC9CDDBFF},
        {"scrollbarHover", 0x525A7AFF, 0xA9AEC2FF},
        {"track", 0x3A4058FF, 0xCDD1DDFF},
        {"knob", 0xFFFFFFFF, 0xFFFFFFFF},
        {"text", 0xE8EAF2FF, 0x1B1E2BFF},
        {"textMuted", 0xA3A8BFFF, 0x5C6380FF},
        {"textDisabled", 0x6A7090FF, 0xA3A8BFFF},
        {"onTooltip", 0xE8EAF2FF, 0xF4F5F9FF},
        {"accent", 0x4C7DFFFF, 0x3A66E0FF},
        {"accentHover", 0x6690FFFF, 0x4C7DFFFF},
        {"accentStrong", 0x3A66E0FF, 0x2C52C0FF},
        {"onAccent", 0xFFFFFFFF, 0xFFFFFFFF},
        {"accentBackground", 0x4C7DFF26, 0x3A66E01F},
        {"accentText", 0x8FB0FFFF, 0x2C52C0FF},
        {"success", 0x3DBE7AFF, 0x2E9E62FF},
        {"onSuccess", 0xFFFFFFFF, 0xFFFFFFFF},
        {"successBackground", 0x3DBE7A26, 0x2E9E621F},
        {"successText", 0x6FDCA0FF, 0x1F7A49FF},
        {"warning", 0xF2B23AFF, 0xD9941CFF},
        {"onWarning", 0x1B1E2BFF, 0xFFFFFFFF},
        {"warningBackground", 0xF2B23A26, 0xD9941C1F},
        {"warningText", 0xF7CB70FF, 0x9A6508FF},
        {"danger", 0xE5534BFF, 0xD0433BFF},
        {"dangerHover", 0xEE6B64FF, 0xE5534BFF},
        {"dangerStrong", 0xC8423BFF, 0xB0352EFF},
        {"onDanger", 0xFFFFFFFF, 0xFFFFFFFF},
        {"dangerBackground", 0xE5534B26, 0xD0433B1F},
        {"dangerText", 0xFF8A84FF, 0xA8322BFF},
        {"information", 0x3AA8E0FF, 0x2A8CC0FF},
        {"onInformation", 0xFFFFFFFF, 0xFFFFFFFF},
        {"informationBackground", 0x3AA8E026, 0x2A8CC01F},
        {"informationText", 0x7FCBF2FF, 0x1D6A93FF},
    }};
    static constexpr std::array<MetricRole, kMetricCount> kMetricRoles{{
        {"controlHeight", 64.0F},
        {"controlRadius", 12.0F},
        {"controlPaddingX", 24.0F},
        {"controlPaddingY", 12.0F},
        {"itemSpacing", 16.0F},
        {"panelPadding", 28.0F},
        {"borderWidth", 2.0F},
        {"focusWidth", 3.0F},
        {"scrollbarSize", 16.0F},
        {"iconSize", 36.0F},
        {"choiceSize", 36.0F},
        {"sliderTrackHeight", 10.0F},
        {"sliderKnobSize", 34.0F},
        {"toggleWidth", 72.0F},
        {"toggleHeight", 38.0F},
        {"progressHeight", 22.0F},
        {"badgePaddingX", 14.0F},
        {"badgePaddingY", 4.0F},
        {"tabPaddingX", 24.0F},
        {"listRowHeight", 64.0F},
        {"dialogWidth", 760.0F},
        {"toastWidth", 560.0F},
        {"tooltipWidth", 520.0F},
        {"settingsLabelWidth", 420.0F},
        {"caretWidth", 2.0F},
        {"circularProgressSize", 72.0F},
        {"circularProgressThickness", 8.0F},
        {"slotSize", 96.0F},
        {"windowTitleHeight", 56.0F},
        {"pageIndicatorSize", 14.0F},
        {"cellRadius", 12.0F},
        {"refreshDistance", 120.0F},
        {"toggleKnobInset", 4.0F},
        {"transitionDuration", 0.15F},
        {"toastLimit", 3.0F},
        {"toneBarWidth", 6.0F},
        {"panelRadius", 16.0F},
        {"contentSpacing", 12.0F},
        {"focusGap", 3.0F},
        {"disabledOpacity", 0.5F},
        {"tooltipDelay", 0.5F},
        {"longPressDuration", 0.5F},
        {"shadowSize", 24.0F},
        {"shadowOffset", 6.0F},
        {"chipHeight", 48.0F},
        {"strokeWidth", 2.0F},
        {"rowPadding", 16.0F},
        {"checkRadius", 8.0F},
        {"menuPadding", 8.0F},
        {"splitterSize", 10.0F},
    }};
    static constexpr std::array<std::string_view, kFontCount> kFontNames{"body", "caption", "button", "heading", "title", "monospace"};
    static constexpr std::array<std::string_view, kSurfaceCount> kSurfaceNames{
        "panel",
        "card",
        "dialog",
        "tooltip",
        "toast",
        "banner",
        "button",
        "buttonHover",
        "buttonPressed",
        "buttonPrimary",
        "buttonPrimaryHover",
        "buttonPrimaryPressed",
        "buttonDestructive",
        "buttonDestructiveHover",
        "buttonDestructivePressed",
        "field",
        "fieldFocused",
        "check",
        "checkChecked",
        "track",
        "trackFill",
        "knob",
        "tab",
        "tabSelected",
        "chip",
        "chipSelected",
        "badge",
        "stickBase",
        "stickKnob",
        "touchButton",
        "touchButtonPressed",
        "segment",
        "segmentSelected",
        "menu",
        "window",
        "slot",
        "slotHighlighted",
        "cell",
        "cellHover",
        "cellPressed",
        "cellSelected",
    };
    // clang-format on
    static constexpr std::array<float, kFontCount> kFontSizes{30.0F, 24.0F, 30.0F, 38.0F, 56.0F, 26.0F};

    // The edges and the center of a nine-slice, the pieces a tiled image repeats, and the smallest size a copy of them may have.
    static constexpr std::array<std::size_t, 5> kTiledPieces{1, 3, 4, 5, 7};
    static constexpr float kMinTileSize = 1.0F;

    template <typename Enum, typename Table, typename Projection> [[nodiscard]] static std::optional<Enum> fromName(const Table& names, std::string_view value, Projection projection) noexcept;
    [[nodiscard]] static math::Insets readInsets(const core::Json& value, const std::string& context);
    [[nodiscard]] static math::Rect readRect(const core::Json& value, const std::string& context);
    static void checkTiles(const Image& image, const std::string& context);
    [[nodiscard]] static const core::Json& readSection(const core::Json& document, const char* key);

    std::string name;
    std::array<math::Color, kColorCount> colors{};
    std::array<float, kMetricCount> metrics{};
    std::array<FontStyle, kFontCount> fonts{};
    std::array<std::optional<Image>, kSurfaceCount> surfaces{};
    std::map<std::string, std::string, std::less<>> fontFiles;
    graphics::Texture::Filter imageFilter = graphics::Texture::Filter::Nearest;
};

} // namespace haylen::ui
