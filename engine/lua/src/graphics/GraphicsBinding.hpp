#pragma once

#include <array>
#include <string_view>

#include <optional>

#include "haylen/graphics/Buffer.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/math/Color.hpp"

struct lua_State;

namespace haylen::lua::graphics {

// Installs `haylen.graphics` with the `Buffer`, `Texture`, `RenderTarget`, `Image`, `Font`, `FontFamily`, `Shader` and `VectorImage` classes.
class GraphicsBinding final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 6> kTextureContentFields{"fill", "pixels", "dynamic", "kind", "layers", "depth"};
    static constexpr std::array<std::string_view, 1> kImageTextureFields{"dynamic"};
    static constexpr std::array<std::string_view, 5> kTextureDataFields{"format", "levels", "kind", "layers", "depth"};
    static constexpr std::array<std::string_view, 2> kUpdateFields{"region", "layer"};
    static constexpr std::array<std::string_view, 5> kViewFields{"kind", "mipLevel", "mipLevels", "layer", "layers"};
    static constexpr std::array<std::string_view, 4> kBufferFields{"data", "usage", "update", "label"};
    static constexpr std::array<std::string_view, 3> kCopyFields{"sourceOffset", "offset", "size"};
    static constexpr std::array<std::string_view, 6> kTargetFields{"colors", "depthFormat", "samples", "depthTexture", "depthResolve", "memoryless"};

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

    // Reads the attachments that the options of a render target at the index name besides the sampling of its textures.
    [[nodiscard]] static haylen::graphics::RenderTarget::Options readTargetOptions(lua_State* L, int index);
    static int newTexture(lua_State* L);
    static void pushDataTexture(lua_State* L, int width, int height);

    // Reads the kind, the layers and the depth of a texture of the size from the options at index 3, with the six faces of a cube and of one cube by default.
    [[nodiscard]] static haylen::graphics::Device::TextureData readShape(lua_State* L, int width, int height);

    // Pushes a texture of a kind other than flat from its RGBA pixels or its fill color.
    static void pushLayeredTexture(lua_State* L, const haylen::graphics::Device::TextureData& shape, std::optional<std::string_view> pixels, haylen::math::Color fill, bool dynamic);
    static int viewTexture(lua_State* L);
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
    static int textureKind(lua_State* L);
    static int textureLayers(lua_State* L);
    static int textureDepth(lua_State* L);
    static int textureMipLevels(lua_State* L);

    static int targetTexture(lua_State* L);
    static int targetWidth(lua_State* L);
    static int targetHeight(lua_State* L);
    static int targetTextures(lua_State* L);
    static int targetDepthTexture(lua_State* L);
    static int targetColors(lua_State* L);
    static int targetDepthFormat(lua_State* L);
    static int targetSamples(lua_State* L);
    static int targetMemoryless(lua_State* L);

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
