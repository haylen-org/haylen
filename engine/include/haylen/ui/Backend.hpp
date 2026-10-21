#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <imgui.h>

#include "haylen/2d/graphics/MeshVertex.hpp"
#include "haylen/2d/graphics/NineSlice.hpp"
#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/input/Key.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/text/Style.hpp"

struct ImGuiWindow;

namespace haylen::platform {
struct Event;
}

namespace haylen::graphics {
class Device;
class Image;
class Viewport;
} // namespace haylen::graphics

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::input {
class Input;
}

namespace haylen::text {
class FontFamily;
class RichText;
} // namespace haylen::text

namespace haylen::ui {

class NavigationInput;
class ScrollArbiter;
class TextSession;

// Runs one Dear ImGui context in design coordinates. It feeds input, keeps the ImGui textures on the GPU and turns the draw lists into meshes of a screen canvas drawn over the app.
class Backend final {
  public:
    // The names of the fonts the UI starts with: the default font of the engine and the monospace font that code and other text with columns draw with.
    static constexpr std::string_view kDefaultFontName = "default";
    static constexpr std::string_view kMonospaceFontName = "monospace";

    // The TrueType or OpenType files of a UI font: its regular face, the bold, italic and bold italic faces it may add, left empty when it has none, and the fallback fonts that draw the characters its faces lack, in order.
    struct FontFiles {
        std::vector<std::uint8_t> regular;
        std::vector<std::uint8_t> bold;
        std::vector<std::uint8_t> italic;
        std::vector<std::uint8_t> boldItalic;
        std::vector<std::vector<std::uint8_t>> fallbacks;
    };

    Backend(graphics::Device& graphicsDevice, platform::Window& hostWindow, std::span<const std::uint8_t> defaultFontData, std::span<const std::uint8_t> monospaceFontData);
    ~Backend();

    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;

    void handleEvent(const platform::Event& event, const graphics::Viewport& viewport);

    // Starts a frame over the visible area of the viewport. A frame the app left open, which happens when a script fails halfway, is closed first.
    void beginFrame(float deltaSeconds, const graphics::Viewport& viewport, const input::Input& input, const NavigationInput& navigation);

    // Ends the frame and draws it with the renderer, leaving out the windows that `renderWindow` drew already.
    void render(graphics2d::Renderer& renderer);

    // Draws a window that ended, with its child windows and its popups, before the frame ends, such as the GUIs of a scene that leaves through a transition into an image of its own.
    void renderWindow(graphics2d::Renderer& renderer, const ImGuiWindow& root);
    [[nodiscard]] bool isFrameActive() const noexcept {
        return frameActive;
    }

    // The design units one UI unit spans, which the next frame takes, so the interface grows or shrinks without changing its layout in UI units.
    [[nodiscard]] float getScale() const noexcept {
        return scale;
    }
    void setScale(float value) noexcept {
        scale = value;
    }

    // The points of the screen one UI unit spans in the current frame, from the scale and the density of the screen.
    [[nodiscard]] float getPointsPerUnit() const noexcept {
        return pointsPerUnit;
    }

    // The gestures of the wheels, the trackpads and the fingers that scroll the areas of the UI.
    [[nodiscard]] ScrollArbiter& getScrollArbiter() noexcept {
        return *scrollArbiter;
    }
    [[nodiscard]] const ScrollArbiter& getScrollArbiter() const noexcept {
        return *scrollArbiter;
    }

    // The framebuffer pixels one UI unit covers in the current frame, which pictures rasterize for.
    [[nodiscard]] math::Vec2 getDensity() const noexcept {
        return density;
    }

    // Converts between design coordinates and UI coordinates, whose origin is the top left of the visible area.
    [[nodiscard]] math::Vec2 toUi(math::Vec2 designPoint) const noexcept {
        return (designPoint - origin) / scale;
    }
    [[nodiscard]] math::Vec2 toDesign(math::Vec2 uiPoint) const noexcept {
        return uiPoint * scale + origin;
    }

    // The visible and safe areas in ImGui coordinates, whose origin is the top left of the visible area.
    [[nodiscard]] math::Rect getDisplayRect() const noexcept;
    [[nodiscard]] math::Rect getSafeRect() const noexcept {
        return safeRect;
    }

    // True while the pointer is over an ImGui window, an interactive element or a region a component blocked, so the app can ignore that input.
    [[nodiscard]] bool isUsingPointer() const;
    [[nodiscard]] bool isUsingKeyboard() const;
    void blockPointer(const math::Rect& area);

    // Shows a cursor shape for this frame while the UI has the pointer, such as the cursor a node asks for, in place of the one ImGui chooses.
    void setCursor(platform::Window::Cursor value) noexcept {
        requestedCursor = value;
    }

    // Marks the window being built as transparent to the pointer outside its items and blocked areas, which suits a window that hosts app GUIs.
    void setTransparentWindow();

    // Tells that the item that holds the pointer follows it while held, such as a slider, so a finger that travels keeps it. Every other item lets go of a finger that travels past the slop or shares the screen with another finger, so a swipe or a pinch never clicks it. The windows of `haylen.imgui` keep every finger.
    void keepDrag(ImGuiID item) noexcept {
        dragKeeper = item;
    }
    void markImmediateWindow();

    // Returns an ImGui reference for an app texture, valid until the end of the frame.
    [[nodiscard]] ImTextureRef getTextureReference(const graphics::Texture& texture);

    // Draws with the renderer at this point of the draw list of the window being built, clipped like the items around it, once the frame renders, in a canvas whose coordinates are UI coordinates.
    void addRenderCallback(std::function<void(graphics2d::Renderer& renderer)> draw);

    // Draws a shape in UI coordinates the same way. The shapes, texts, rich texts and nine-slices drawn one after another in a window share one command of its draw list, and the frame keeps them in storage that frames reuse.
    void addShape(const graphics2d::Shape& shape);

    // Draws a text in UI coordinates the same way, with its top left corner at the position.
    void addText(std::shared_ptr<text::FontFamily> family, std::string_view content, math::Vec2 position, const text::Style& style);

    // Draws a rich text in UI coordinates the same way, at a scale and tinted by a color.
    void addRichText(std::shared_ptr<text::RichText> richText, math::Vec2 position, math::Vec2 textScale, math::Color tint);

    // Draws a nine-slice over an area in UI coordinates the same way, laid out with its border scale and then mapped by a scale and an offset, such as those of the nodes around it.
    void addNineSlice(const graphics2d::NineSlice& slice, const math::Rect& area, float borderScale, math::Vec2 areaScale, math::Vec2 offset, math::Color color);

    // The shapes the frame drew so far, in UI coordinates and in the order they were drawn, such as for checks of where a component draws.
    [[nodiscard]] std::span<const graphics2d::Shape> getShapes() const noexcept {
        return shapes;
    }

    // A text the frame drew, whose characters `getContent` returns.
    struct Text {
        std::shared_ptr<text::FontFamily> family;
        std::size_t begin = 0;
        std::size_t length = 0;
        math::Vec2 origin;
        text::Style style;
    };

    // The texts the frame drew so far, in UI coordinates and in the order they were drawn.
    [[nodiscard]] std::span<const Text> getTexts() const noexcept {
        return texts;
    }
    [[nodiscard]] std::string_view getContent(const Text& drawn) const noexcept {
        return std::string_view(characters).substr(drawn.begin, drawn.length);
    }

    // Begins a child window that draws at this point of the draw list of the window being built, so what that window draws after it, such as the nodes and GUIs above a scroll, covers it. The child ends with `ImGui::EndChild`.
    bool beginChild(const char* id, math::Vec2 size, ImGuiChildFlags childFlags, ImGuiWindowFlags windowFlags);

    // Adds a font under a name and returns its regular face. Fonts are sized when drawn, so one font serves every size, and a fallback draws the characters a face lacks with its em square as large as the em square of that face.
    ImFont* addFont(const std::string& name, FontFiles files);

    // Returns the face of a font for a style, which is the regular face for a style the font has no face for.
    [[nodiscard]] ImFont* getFont(std::string_view name, bool bold = false, bool italic = false) const;

    // ImGui sizes a font by the height of a face from its ascent to its descent, so this returns that height for the face of a style at the size of its em square, which is the size ImGui draws it at.
    [[nodiscard]] float getFontHeight(std::string_view name, float size, bool bold = false, bool italic = false) const;
    [[nodiscard]] bool hasFont(std::string_view name) const {
        return fonts.contains(name);
    }

    [[nodiscard]] ImGuiContext* getImGuiContext() const noexcept {
        return imguiContext;
    }
    void makeCurrent() const;

    // The text fields of the UI edit through this session, which keeps the focused field in step with the text input of the window.
    [[nodiscard]] TextSession& getTextSession() noexcept;

    // How far up the UI moves so the focused text field stays above the on-screen keyboard, in UI units.
    [[nodiscard]] float getKeyboardOffset() const noexcept;

    // The display and safe areas moved up by the keyboard offset, where every GUI places what it draws, its anchored nodes, dialogs, windows, toasts and popups alike, so the whole UI moves together and a focused field always moves with it.
    [[nodiscard]] math::Rect getLiftedDisplayRect() const noexcept;
    [[nodiscard]] math::Rect getLiftedSafeRect() const noexcept;

  private:
    struct Recovery;

    // A face of a UI font and its height from ascent to descent in em squares.
    struct Face {
        ImFont* font = nullptr;
        float height = 1.0F;
    };

    // The faces of a UI font, without a font for a style it has no face for.
    struct Typeface {
        Face regular;
        Face bold;
        Face italic;
        Face boldItalic;
    };

    // ImGui copies the data of a callback command into the draw list byte by byte, so a command carries the index of its function instead of the function.
    struct RenderCall {
        Backend* owner = nullptr;
        std::size_t index = 0;
    };

    struct ChildCall {
        Backend* owner = nullptr;
        const ImGuiWindow* window = nullptr;
    };

    // A draw of the frame that a run of a callback command draws, one of its shapes, texts, rich texts or nine-slices.
    struct Draw {
        enum class Kind : std::uint8_t { Shape, Text, RichText, Sprites };

        Kind kind = Kind::Shape;
        std::size_t index = 0;
    };

    struct RichTextDraw {
        std::shared_ptr<text::RichText> richText;
        math::Vec2 origin;
        math::Vec2 scale;
        math::Color tint;
    };

    struct SpriteDraw {
        graphics::Texture texture;
        std::size_t first = 0;
        std::size_t count = 0;
    };

    // The draws a callback command draws, a run of the draws of the frame.
    struct DrawRun {
        Backend* owner = nullptr;
        std::size_t first = 0;
        std::size_t count = 0;
    };

    // App textures shown by the UI get identifiers with the top bit set, so they never collide with the atlas textures ImGui asks for.
    static constexpr ImTextureID kAppTextureBit = ImTextureID{1} << 63U;
    static constexpr float kBaseFontSize = 28.0F;
    // ImGui windows of the app scroll by this many font sizes for each step of a wheel.
    static constexpr float kImGuiScrollFonts = 5.0F;
    static constexpr std::size_t kFontHeaderSize = 12;

    // The farthest in pixels of the screen that ImGui lets the segments of a curve stray from the true curve.
    static constexpr float kCurveTolerance = 0.2F;

    [[nodiscard]] static ImGuiKey toImGuiKey(input::Key key) noexcept;
    [[nodiscard]] static bool isEditKey(ImGuiKey key) noexcept;
    [[nodiscard]] static platform::Window::Cursor toCursor(ImGuiMouseCursor value) noexcept;
    [[nodiscard]] static graphics::Image toImage(ImTextureData& texture);
    [[nodiscard]] static math::Color toColor(ImU32 value) noexcept;
    [[nodiscard]] static float readHeight(const std::string& name, std::span<const std::uint8_t> data);
    [[nodiscard]] static Backend& getOwner(ImGuiContext* context);
    [[nodiscard]] static const char* getClipboardText(ImGuiContext* context);
    static void setClipboardText(ImGuiContext* context, const char* text);
    static void resetRenderState(const ImDrawList* list, const ImDrawCmd* command);
    static void runRenderCall(const ImDrawList* list, const ImDrawCmd* command);
    static void runChildCall(const ImDrawList* list, const ImDrawCmd* command);
    static void runDraws(const ImDrawList* list, const ImDrawCmd* command);

    [[nodiscard]] Face addFace(const std::string& name, std::vector<std::uint8_t> bytes, std::span<const std::span<std::uint8_t>> fallbacks);
    [[nodiscard]] const Face& findFace(std::string_view name, bool bold, bool italic) const;
    void handlePointer(const platform::Event& event, const graphics::Viewport& viewport);
    void handleTextAction(const platform::Event& event);
    void feedGamepad(const input::Input& input, const NavigationInput& navigation);

    // Closes every popup that no component drew in the last frame, such as the dialog of a node that was hidden or of a GUI that was unmounted, so it never keeps the pointer and cancel from the windows under it.
    static void closeAbandonedPopups();
    void releaseTravelledTouch();
    void updateTextures(ImDrawData& data);
    static void collectLists(const ImGuiWindow& window, std::vector<const ImDrawList*>& lists);
    void drawLists(graphics2d::Renderer& renderer, std::span<const ImDrawList* const> lists);
    void drawWindow(const ImGuiWindow& imguiWindow);
    void addDraw(Draw::Kind kind, std::size_t index);
    void runDraw(const Draw& draw);
    void drawList(const ImDrawList& list);
    [[nodiscard]] const graphics::Texture* findTexture(ImTextureID id) const;

    graphics::Device& device;
    platform::Window& window;
    std::unique_ptr<Recovery> recovery;
    std::unique_ptr<TextSession> textSession;
    std::unique_ptr<ScrollArbiter> scrollArbiter;
    ImGuiContext* imguiContext = nullptr;
    std::vector<std::vector<std::uint8_t>> fontData;
    std::map<std::string, Typeface, std::less<>> fonts;
    std::unordered_map<ImTextureID, graphics::Texture> atlasTextures;
    // The app textures the frame shows, few enough to find by a walk, in storage that frames reuse.
    std::vector<graphics::Texture> frameTextures;
    std::vector<std::function<void(graphics2d::Renderer&)>> renderCalls;
    std::vector<Draw> draws;
    std::vector<graphics2d::Shape> shapes;
    std::vector<Text> texts;
    std::string characters;
    std::vector<RichTextDraw> richTexts;
    std::vector<SpriteDraw> spriteDraws;
    std::vector<graphics2d::SpriteInstance> sprites;
    std::vector<graphics2d::NineSlice::Patch> patches;
    std::vector<const ImDrawList*> renderedLists;
    std::vector<const ImDrawList*> drawnLists;
    std::vector<graphics2d::MeshVertex> meshVertices;
    std::vector<std::uint32_t> meshIndices;
    graphics2d::Renderer* rendering = nullptr;
    ImTextureID nextAtlasTexture = 1;
    math::Vec2 origin;
    float scale = 1.0F;
    float pointsPerUnit = 1.0F;
    math::Vec2 density{1.0F, 1.0F};
    math::Rect safeRect;
    std::vector<math::Rect> blocked;
    std::vector<math::Rect> blockedPrevious;
    ImGuiID transparentWindow = 0;
    ImGuiID dragKeeper = 0;
    std::vector<ImGuiID> immediateWindows;
    std::optional<std::uint64_t> primaryTouch;
    bool touchShared = false;
    std::optional<math::Vec2> pointerPosition;
    std::string clipboard;
    platform::Window::Cursor cursor = platform::Window::Cursor::Default;
    std::optional<platform::Window::Cursor> requestedCursor;
    bool keyboardShown = false;
    bool frameActive = false;
};

} // namespace haylen::ui
