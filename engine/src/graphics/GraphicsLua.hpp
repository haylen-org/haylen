#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::graphics {

// Installs haylen.graphics with the Texture, RenderTarget, Font, FontFamily and Shader classes.
class GraphicsLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 3> kTextureContentFields{"fill", "pixels", "dynamic"};
    static constexpr std::array<std::string_view, 1> kImageTextureFields{"dynamic"};

    static int newRenderTarget(lua_State* L);
    static int newTexture(lua_State* L);

    // Pushes the texture of the image file whose bytes lie at index 1, with the options at index 2.
    static void pushImageTexture(lua_State* L);
    static int updateTexture(lua_State* L);
    static int maxTextureSize(lua_State* L);
    static int whiteTexture(lua_State* L);
    static int backendName(lua_State* L);

    static int textureWidth(lua_State* L);
    static int textureHeight(lua_State* L);
    static int textureFilter(lua_State* L);
    static int textureWrap(lua_State* L);

    static int targetTexture(lua_State* L);
    static int targetWidth(lua_State* L);
    static int targetHeight(lua_State* L);

    static int shaderName(lua_State* L);
    static int shaderUniforms(lua_State* L);
    static int shaderTextures(lua_State* L);

    static int open(lua_State* L);
};

} // namespace haylen::graphics
