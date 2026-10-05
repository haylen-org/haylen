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
#include "haylen/graphics/Texture.hpp"
#include "haylen/input/Key.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Window.hpp"

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

namespace haylen::ui {

class NavigationInput;
class TextSession;

// Runs one Dear ImGui context in design coordinates. It feeds input, keeps the ImGui textures on the GPU and turns the draw lists into meshes of a screen canvas drawn over the app.
class Backend final {
  public:
    // The name of the font the UI starts with, the default font of the engine.
    static constexpr std::string_view kDefaultFontName = "default";

    // The TrueType or OpenType files of a UI font: its regular face, the bold, italic and bold italic faces it may add, left empty when it has none, and the fallback fonts that draw the characters its faces lack, in order.
    struct FontFiles {
        std::vector<std::uint8_t> regular;
        std::vector<std::uint8_t> bold;
        std::vector<std::uint8_t> italic;
        std::vector<std::uint8_t> boldItalic;
        std::vector<std::vector<std::uint8_t>> fallbacks;
    };

    Backend(graphics::Device& graphicsDevice, platform::Window& hostWindow, std::span<const std::uint8_t> defaultFontData);
    ~Backend();

    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;

    void handleEvent(const platform::Event& event, const graphics::Viewport& viewport);

    // Starts a frame over the visible area of the viewport. A frame the app left open, which happens when a script fails halfway, is closed first.
    void beginFrame(float deltaSeconds, const graphics::Viewport& viewport, const input::Input& input, const NavigationInput& navigation);

    // Ends the frame and draws it with the renderer, leaving out the windows that `renderWindow` drew already.
    void render(graphics2d::Renderer& renderer);

    // Draws a window that ended, with its child windows and its popups, before the frame ends, such as the documents of a scene that leaves through a transition into an image of its own.
    void renderWindow(graphics2d::Renderer& renderer, const ImGuiWindow& root);
    [[nodiscard]] bool isFrameActive() const noexcept {
        return frameActive;
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

    // Marks the window being built as transparent to the pointer outside its items and blocked areas, which suits a window that hosts app UI documents.
    void setTransparentWindow();

    // Returns an ImGui reference for an app texture, valid until the end of the frame.
    [[nodiscard]] ImTextureRef getTextureReference(const graphics::Texture& texture);

    // Draws with the renderer at this point of the draw list of the window being built, clipped like the items around it, once the frame renders. The function receives the offset from UI coordinates to the screen canvas it draws in.
    void addRenderCallback(std::function<void(graphics2d::Renderer& renderer, math::Vec2 offset)> draw);

    // Adds a font under a name and returns its regular face. Fonts are sized when drawn, so one font serves every size, and a fallback draws the characters a face lacks with its em square as large as the em square of that face.
    ImFont* addFont(const std::string& name, FontFiles files);

    // Returns the face of a font for a style, which is the regular face for a style the font has no face for.
    [[nodiscard]] ImFont* getFont(std::string_view name, bool bold = false, bool italic = false) const;

    // ImGui sizes a font by its height from ascent to descent, so this returns the size of the em square of the regular face at a UI size, which text drawn beside ImGui, such as rich text, takes to match it.
    [[nodiscard]] float getEmSize(std::string_view name, float size) const;
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

  private:
    struct Recovery;

    // The faces of a UI font, null for a style it has no face for, and how many times its em square fits in the height of its regular face.
    struct Typeface {
        ImFont* regular = nullptr;
        ImFont* bold = nullptr;
        ImFont* italic = nullptr;
        ImFont* boldItalic = nullptr;
        float emRatio = 1.0F;
    };

    // ImGui copies the data of a callback command into the draw list byte by byte, so a command carries the index of its function instead of the function.
    struct RenderCall {
        Backend* owner = nullptr;
        std::size_t index = 0;
    };

    // App textures shown by the UI get identifiers with the top bit set, so they never collide with the atlas textures ImGui asks for.
    static constexpr ImTextureID kAppTextureBit = ImTextureID{1} << 63U;
    static constexpr float kBaseFontSize = 28.0F;
    static constexpr std::size_t kFontHeaderSize = 12;

    [[nodiscard]] static ImGuiKey toImGuiKey(input::Key key) noexcept;
    [[nodiscard]] static bool isEditKey(ImGuiKey key) noexcept;
    [[nodiscard]] static platform::Window::Cursor toCursor(ImGuiMouseCursor value) noexcept;
    [[nodiscard]] static graphics::Image toImage(ImTextureData& texture);
    [[nodiscard]] static math::Color toColor(ImU32 value) noexcept;
    [[nodiscard]] static float getEmRatio(const std::string& name, std::span<const std::uint8_t> data);
    [[nodiscard]] static Backend& getOwner(ImGuiContext* context);
    [[nodiscard]] static const char* getClipboardText(ImGuiContext* context);
    static void setClipboardText(ImGuiContext* context, const char* text);
    static void resetRenderState(const ImDrawList* list, const ImDrawCmd* command);
    static void runRenderCall(const ImDrawList* list, const ImDrawCmd* command);

    ImFont* addFace(const std::string& name, std::vector<std::uint8_t> bytes, std::span<const std::span<std::uint8_t>> fallbacks);
    void handlePointer(const platform::Event& event, const graphics::Viewport& viewport);
    void handleTextAction(const platform::Event& event);
    void feedGamepad(const input::Input& input, const NavigationInput& navigation);

    // Closes every popup that no component drew in the last frame, such as the dialog of a node that was hidden or of a document that was unmounted, so it never keeps the pointer and cancel from the windows under it.
    static void closeAbandonedPopups();
    void updateTextures(ImDrawData& data);
    static void collectLists(const ImGuiWindow& window, std::vector<const ImDrawList*>& lists);
    void drawLists(graphics2d::Renderer& renderer, std::span<const ImDrawList* const> lists);
    [[nodiscard]] const graphics::Texture* findTexture(ImTextureID id) const;

    graphics::Device& device;
    platform::Window& window;
    std::unique_ptr<Recovery> recovery;
    std::unique_ptr<TextSession> textSession;
    ImGuiContext* imguiContext = nullptr;
    std::vector<std::vector<std::uint8_t>> fontData;
    std::map<std::string, Typeface, std::less<>> fonts;
    std::unordered_map<ImTextureID, graphics::Texture> atlasTextures;
    std::unordered_map<ImTextureID, graphics::Texture> frameTextures;
    std::vector<std::function<void(graphics2d::Renderer&, math::Vec2)>> renderCalls;
    std::vector<const ImDrawList*> renderedLists;
    std::vector<graphics2d::MeshVertex> meshVertices;
    std::vector<std::uint32_t> meshIndices;
    graphics2d::Renderer* rendering = nullptr;
    ImTextureID nextAtlasTexture = 1;
    math::Vec2 origin;
    math::Rect safeRect;
    std::vector<math::Rect> blocked;
    std::vector<math::Rect> blockedPrevious;
    ImGuiID transparentWindow = 0;
    std::optional<std::uint64_t> primaryTouch;
    std::optional<math::Vec2> pointerPosition;
    std::string clipboard;
    ImGuiMouseCursor cursor = ImGuiMouseCursor_Arrow;
    bool keyboardShown = false;
    bool frameActive = false;
};

} // namespace haylen::ui
