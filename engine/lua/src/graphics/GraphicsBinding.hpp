#pragma once

#include <array>
#include <string_view>

#include "haylen/graphics/Buffer.hpp"

struct lua_State;

namespace haylen::lua::graphics {

// Installs `haylen.graphics` with the `Buffer`, `Texture`, `RenderTarget`, `Image`, `Font`, `FontFamily`, `Shader` and `VectorImage` classes.
class GraphicsBinding final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 3> kTextureContentFields{"fill", "pixels", "dynamic"};
    static constexpr std::array<std::string_view, 1> kImageTextureFields{"dynamic"};
    static constexpr std::array<std::string_view, 2> kTextureDataFields{"format", "levels"};
    static constexpr std::array<std::string_view, 1> kUpdateFields{"region"};
    static constexpr std::array<std::string_view, 4> kBufferFields{"data", "usage", "update", "label"};
    static constexpr std::array<std::string_view, 3> kCopyFields{"sourceOffset", "offset", "size"};

    static int newBuffer(lua_State* L);
    [[nodiscard]] static haylen::graphics::Buffer::Usage readUsage(lua_State* L, int index);
    static int bufferSize(lua_State* L);
    static int bufferUsage(lua_State* L);
    static int bufferUpdate(lua_State* L);
    static int bufferLabel(lua_State* L);
    static int writeBuffer(lua_State* L);
    static int copyBuffer(lua_State* L);
    static int readBufferAsync(lua_State* L);

    static int newRenderTarget(lua_State* L);
    static int newTexture(lua_State* L);
    static void pushDataTexture(lua_State* L, int width, int height);
    [[nodiscard]] static bool hasField(lua_State* L, int index, const char* name);

    // Pushes the texture of the image file whose bytes lie at index 1, with the options at index 2.
    static void pushImageTexture(lua_State* L);
    static int updateTexture(lua_State* L);
    static int maxTextureSize(lua_State* L);
    static int trim(lua_State* L);
    static void setField(lua_State* L, const char* name, int value);
    static void setField(lua_State* L, const char* name, bool value);
    static int capabilities(lua_State* L);
    static int formatSupport(lua_State* L);
    static int depthRange(lua_State* L);
    static int whiteTexture(lua_State* L);
    static int apiName(lua_State* L);

    static int textureWidth(lua_State* L);
    static int textureHeight(lua_State* L);
    static int textureFilter(lua_State* L);
    static int textureWrapU(lua_State* L);
    static int textureWrapV(lua_State* L);
    static int textureWrapW(lua_State* L);
    static int textureAnisotropy(lua_State* L);
    static int textureLodBias(lua_State* L);
    static int textureMinLod(lua_State* L);
    static int textureMaxLod(lua_State* L);
    static int textureBorderColor(lua_State* L);
    static int textureMipmaps(lua_State* L);
    static int textureFormat(lua_State* L);

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
