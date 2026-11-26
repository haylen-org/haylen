#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::lua::graphics {

// Installs `haylen.graphics` with the `Texture`, `RenderTarget`, `Image`, `Font`, `FontFamily`, `Shader` and `VectorImage` classes.
class GraphicsBinding final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 3> kTextureContentFields{"fill", "pixels", "dynamic"};
    static constexpr std::array<std::string_view, 1> kImageTextureFields{"dynamic"};
    static constexpr std::array<std::string_view, 1> kUpdateFields{"region"};

    static int newRenderTarget(lua_State* L);
    static int newTexture(lua_State* L);

    // Pushes the texture of the image file whose bytes lie at index 1, with the options at index 2.
    static void pushImageTexture(lua_State* L);
    static int updateTexture(lua_State* L);
    static int maxTextureSize(lua_State* L);
    static int trim(lua_State* L);
    static int maxVolumeSize(lua_State* L);
    static int maxCubeSize(lua_State* L);
    static int sampleCounts(lua_State* L);
    static int depthRange(lua_State* L);
    static int whiteTexture(lua_State* L);
    static int apiName(lua_State* L);

    static int textureWidth(lua_State* L);
    static int textureHeight(lua_State* L);
    static int textureFilter(lua_State* L);
    static int textureWrap(lua_State* L);
    static int textureMipmaps(lua_State* L);

    static int targetTexture(lua_State* L);
    static int targetWidth(lua_State* L);
    static int targetHeight(lua_State* L);

    static int shaderName(lua_State* L);
    static int shaderUniforms(lua_State* L);
    static int shaderTextures(lua_State* L);

    static int newVectorImage(lua_State* L);
    static int vectorWidth(lua_State* L);
    static int vectorHeight(lua_State* L);
    static int rasterizeVector(lua_State* L);

    static int open(lua_State* L);
};

} // namespace haylen::lua::graphics
