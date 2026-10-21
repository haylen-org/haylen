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
        ScrollbarGap,
        ScrollbarInset,
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
        ToastRadius,
        ToastPeek,
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
        ScrollLine,
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
        Radio,
        RadioSelected,
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
        TitleBar,
        Close,
        CloseHover,
        ClosePressed,
        Slot,
        SlotHighlighted,
        Cell,
        CellHover,
        CellPressed,
        CellSelected,
        Scrollbar,
        ScrollbarHover,
        ScrollbarTrack,
        FocusRing,
    };

    // The font a role draws with, its size and the style of its face, where a font without a face for the style draws with its regular face.
    struct FontStyle {
        std::string font = "default";
        float size = 30.0F;
        bool bold = false;
        bool italic = false;
    };

    // A nine-slice image drawn in place of a flat surface. Borders keep their texture size times the scale, and the padding moves content away from thick borders. An SVG document takes the place of the texture through `vector`, with its regions in the units of the document, and the whole document cut by `borders` when `whole` is set, since its size is known only once it loads.
    struct Image {
        graphics2d::NineSlice slice;
        float scale = 1.0F;
        math::Insets padding;
        math::Color tint = math::Color::white();
        bool colorize = false;
        std::string vector;
        math::Insets borders;
        bool whole = false;
    };

    // Loads a texture for a theme image from a path relative to the package `content/` folder.
    using TextureLoader = std::function<graphics::Texture(std::string_view path, graphics::Texture::Options options)>;

    static constexpr std::size_t kColorCount = static_cast<std::size_t>(Color::InformationText) + 1;
    static constexpr std::size_t kMetricCount = static_cast<std::size_t>(Metric::ScrollLine) + 1;
    static constexpr std::size_t kFontCount = static_cast<std::size_t>(Font::Monospace) + 1;
    static constexpr std::size_t kSurfaceCount = static_cast<std::size_t>(Surface::FocusRing) + 1;

    // The built-in themes. The dark and light themes name their high contrast variants, which keep their metrics and fonts with colors of stronger contrast.
    [[nodiscard]] static Theme dark();
    [[nodiscard]] static Theme light();
    [[nodiscard]] static Theme darkHighContrast();
    [[nodiscard]] static Theme lightHighContrast();

    // Reads a theme on top of a base theme. The JSON holds `name`, optional `colors`, `metrics`, `fonts`, `fontFiles`, `surfaces`, `imageFilter` and `highContrast`, which names the theme to use while the system asks for higher contrast, or is `false` to keep this one, and `fontFiles` maps font names to TrueType files the caller registers before drawing.
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
    // The name of the theme the UI draws with instead of this one while the system asks for higher contrast, or empty to keep this one.
    [[nodiscard]] const std::string& getHighContrast() const noexcept {
        return highContrast;
    }
    void setHighContrast(std::string value) {
        highContrast = std::move(value);
    }

    // Takes the transparency out of the colors of the surfaces, the window, panels, raised surfaces and tooltips, which the UI draws while the system asks for reduced transparency.
    void makeSurfacesOpaque() noexcept;

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

    // Copies the colors and shapes into an ImGui style, so immediate windows match the retained components, at a scale where one UI unit spans the given points of the screen.
    void applyTo(ImGuiStyle& style, float pointsPerUnit) const;

  private:
    // A color role with its name and its colors in the dark and light themes and in their high contrast variants, as `0xRRGGBBAA`.
    struct ColorRole {
        std::string_view name;
        std::uint32_t dark = 0;
        std::uint32_t light = 0;
        std::uint32_t darkHighContrast = 0;
        std::uint32_t lightHighContrast = 0;
    };

    // A metric with its name and its value in the built-in themes, which suits the 1920 by 1080 design resolution the engine uses by default.
    struct MetricRole {
        std::string_view name;
        float value = 0.0F;
    };

    // clang-format off
    static constexpr std::array<ColorRole, kColorCount> kColorRoles{{
        {"window", 0x1B1E2BFF, 0xF4F5F9FF, 0x000000FF, 0xFFFFFFFF},
        {"panel", 0x232739FF, 0xFFFFFFFF, 0x000000FF, 0xFFFFFFFF},
        {"raised", 0x2C3147FF, 0xFFFFFFFF, 0x101010FF, 0xFFFFFFFF},
        {"tooltip", 0x0F111AFF, 0x1B1E2BFF, 0x000000FF, 0x000000FF},
        {"overlay", 0x000000A0, 0x00000066, 0x000000D0, 0x000000A0},
        {"shadow", 0x00000066, 0x1B1E2B29, 0x00000066, 0x00000040},
        {"hover", 0xFFFFFF14, 0x0000000F, 0xFFFFFF33, 0x0000001F},
        {"pressed", 0xFFFFFF24, 0x0000001F, 0xFFFFFF4D, 0x00000033},
        {"selection", 0x4C7DFF40, 0x4C7DFF33, 0xFFD60A66, 0x0040C04D},
        {"focus", 0x7AA2FFFF, 0x3A66E0FF, 0xFFD60AFF, 0x0040C0FF},
        {"caret", 0x7AA2FFFF, 0x3A66E0FF, 0xFFD60AFF, 0x0040C0FF},
        {"border", 0x3A4058FF, 0xD6D9E4FF, 0xFFFFFFFF, 0x000000FF},
        {"borderStrong", 0x525A7AFF, 0xB3B8CCFF, 0xFFFFFFFF, 0x000000FF},
        {"scrollbar", 0x3A4058FF, 0xC9CDDBFF, 0xC0C0C0FF, 0x404040FF},
        {"scrollbarHover", 0x525A7AFF, 0xA9AEC2FF, 0xFFFFFFFF, 0x000000FF},
        {"track", 0x3A4058FF, 0xCDD1DDFF, 0x8A8A8AFF, 0x6A6A6AFF},
        {"knob", 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
        {"text", 0xE8EAF2FF, 0x1B1E2BFF, 0xFFFFFFFF, 0x000000FF},
        {"textMuted", 0xA3A8BFFF, 0x5C6380FF, 0xE6E6E6FF, 0x1F1F1FFF},
        {"textDisabled", 0x6A7090FF, 0xA3A8BFFF, 0xA0A0A0FF, 0x5E5E5EFF},
        {"onTooltip", 0xE8EAF2FF, 0xF4F5F9FF, 0xFFFFFFFF, 0xFFFFFFFF},
        {"accent", 0x4C7DFFFF, 0x3A66E0FF, 0x9EC3FFFF, 0x0038B0FF},
        {"accentHover", 0x6690FFFF, 0x4C7DFFFF, 0xC2DAFFFF, 0x0048D0FF},
        {"accentStrong", 0x3A66E0FF, 0x2C52C0FF, 0x7AACFFFF, 0x002A88FF},
        {"onAccent", 0xFFFFFFFF, 0xFFFFFFFF, 0x000000FF, 0xFFFFFFFF},
        {"accentBackground", 0x4C7DFF26, 0x3A66E01F, 0x9EC3FF40, 0x0038B026},
        {"accentText", 0x8FB0FFFF, 0x2C52C0FF, 0xB8D3FFFF, 0x002A88FF},
        {"success", 0x3DBE7AFF, 0x2E9E62FF, 0x5EE39AFF, 0x0B6B38FF},
        {"onSuccess", 0xFFFFFFFF, 0xFFFFFFFF, 0x000000FF, 0xFFFFFFFF},
        {"successBackground", 0x3DBE7A26, 0x2E9E621F, 0x5EE39A40, 0x0B6B3826},
        {"successText", 0x6FDCA0FF, 0x1F7A49FF, 0x8CF0B8FF, 0x0B5A30FF},
        {"warning", 0xF2B23AFF, 0xD9941CFF, 0xFFD36BFF, 0x7A4D00FF},
        {"onWarning", 0x1B1E2BFF, 0xFFFFFFFF, 0x000000FF, 0xFFFFFFFF},
        {"warningBackground", 0xF2B23A26, 0xD9941C1F, 0xFFD36B40, 0x7A4D0026},
        {"warningText", 0xF7CB70FF, 0x9A6508FF, 0xFFE19AFF, 0x6A4300FF},
        {"danger", 0xE5534BFF, 0xD0433BFF, 0xFF8A84FF, 0x9E1C14FF},
        {"dangerHover", 0xEE6B64FF, 0xE5534BFF, 0xFFA8A3FF, 0xB3261EFF},
        {"dangerStrong", 0xC8423BFF, 0xB0352EFF, 0xFF6B63FF, 0x80140EFF},
        {"onDanger", 0xFFFFFFFF, 0xFFFFFFFF, 0x000000FF, 0xFFFFFFFF},
        {"dangerBackground", 0xE5534B26, 0xD0433B1F, 0xFF8A8440, 0x9E1C1426},
        {"dangerText", 0xFF8A84FF, 0xA8322BFF, 0xFFB0ACFF, 0x8A1810FF},
        {"information", 0x3AA8E0FF, 0x2A8CC0FF, 0x7FD1FFFF, 0x0B5480FF},
        {"onInformation", 0xFFFFFFFF, 0xFFFFFFFF, 0x000000FF, 0xFFFFFFFF},
        {"informationBackground", 0x3AA8E026, 0x2A8CC01F, 0x7FD1FF40, 0x0B548026},
        {"informationText", 0x7FCBF2FF, 0x1D6A93FF, 0xA8E0FFFF, 0x0A4A70FF},
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
        {"scrollbarSize", 10.0F},
        {"scrollbarGap", 8.0F},
        {"scrollbarInset", 4.0F},
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
        {"toastRadius", 999.0F},
        {"toastPeek", 14.0F},
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
        {"scrollLine", 64.0F},
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
        "radio",
        "radioSelected",
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
        "titleBar",
        "close",
        "closeHover",
        "closePressed",
        "slot",
        "slotHighlighted",
        "cell",
        "cellHover",
        "cellPressed",
        "cellSelected",
        "scrollbar",
        "scrollbarHover",
        "scrollbarTrack",
        "focusRing",
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
    std::string highContrast;
    std::array<math::Color, kColorCount> colors{};
    std::array<float, kMetricCount> metrics{};
    std::array<FontStyle, kFontCount> fonts{};
    std::array<std::optional<Image>, kSurfaceCount> surfaces{};
    std::map<std::string, std::string, std::less<>> fontFiles;
    graphics::Texture::Filter imageFilter = graphics::Texture::Filter::Nearest;
};

} // namespace haylen::ui
