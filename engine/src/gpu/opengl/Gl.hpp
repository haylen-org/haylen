#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::opengl {

// The entry points of OpenGL 4.3 core, OpenGL ES 3.0 and WebGL2 that the device calls, loaded through the context of the platform, with the constants and the types they take. The flavour decides which entry points exist, and the extensions of the context add the optional ones, whose pointers stay null where the context lacks them.
class Gl final {
  public:
    using Enum = std::uint32_t;
    using Bitfield = std::uint32_t;
    using Uint = std::uint32_t;
    using Int = std::int32_t;
    using Sizei = std::int32_t;
    using Boolean = std::uint8_t;
    using Float = float;
    using IntPtr = std::intptr_t;
    using SizeiPtr = std::intptr_t;
    using Uint64 = std::uint64_t;
    struct SyncObject;
    using Sync = SyncObject*;
    using DebugProc = void (*)(Enum source, Enum type, Uint id, Enum severity, Sizei length, const char* message, const void* user);

    static constexpr Enum kNone = 0;
    static constexpr Enum kNoError = 0;
    static constexpr Enum kZero = 0;
    static constexpr Enum kOne = 1;

    static constexpr Enum kByte = 0x1400;
    static constexpr Enum kUnsignedByte = 0x1401;
    static constexpr Enum kShort = 0x1402;
    static constexpr Enum kUnsignedShort = 0x1403;
    static constexpr Enum kInt = 0x1404;
    static constexpr Enum kUnsignedInt = 0x1405;
    static constexpr Enum kFloat = 0x1406;
    static constexpr Enum kHalfFloat = 0x140B;
    static constexpr Enum kUnsignedInt2101010Rev = 0x8368;
    static constexpr Enum kUnsignedInt248 = 0x84FA;
    static constexpr Enum kFloat32UnsignedInt248Rev = 0x8DAD;

    static constexpr Enum kPoints = 0x0000;
    static constexpr Enum kLines = 0x0001;
    static constexpr Enum kLineStrip = 0x0003;
    static constexpr Enum kTriangles = 0x0004;
    static constexpr Enum kTriangleStrip = 0x0005;

    static constexpr Enum kVendor = 0x1F00;
    static constexpr Enum kRenderer = 0x1F01;
    static constexpr Enum kVersion = 0x1F02;
    static constexpr Enum kExtensions = 0x1F03;
    static constexpr Enum kMajorVersion = 0x821B;
    static constexpr Enum kMinorVersion = 0x821C;
    static constexpr Enum kNumExtensions = 0x821D;
    static constexpr Enum kMaxTextureSize = 0x0D33;
    static constexpr Enum kMax3dTextureSize = 0x8073;
    static constexpr Enum kMaxCubeMapTextureSize = 0x851C;
    static constexpr Enum kMaxArrayTextureLayers = 0x88FF;
    static constexpr Enum kMaxSamples = 0x8D57;
    static constexpr Enum kMaxDrawBuffers = 0x8824;
    static constexpr Enum kMaxColorAttachments = 0x8CDF;
    static constexpr Enum kMaxVertexAttribs = 0x8869;
    static constexpr Enum kMaxCombinedTextureImageUnits = 0x8B4D;
    static constexpr Enum kMaxTextureMaxAnisotropy = 0x84FF;
    static constexpr Enum kGpuDisjoint = 0x8FBB;

    static constexpr Enum kBlend = 0x0BE2;
    static constexpr Enum kCullFace = 0x0B44;
    static constexpr Enum kDepthTest = 0x0B71;
    static constexpr Enum kStencilTest = 0x0B90;
    static constexpr Enum kScissorTest = 0x0C11;
    static constexpr Enum kDither = 0x0BD0;
    static constexpr Enum kPolygonOffsetFill = 0x8037;
    static constexpr Enum kSampleAlphaToCoverage = 0x809E;
    static constexpr Enum kFramebufferSrgb = 0x8DB9;
    static constexpr Enum kProgramPointSize = 0x8642;
    static constexpr Enum kTextureCubeMapSeamless = 0x884F;
    static constexpr Enum kDebugOutput = 0x92E0;
    static constexpr Enum kDebugOutputSynchronous = 0x8242;

    static constexpr Enum kFront = 0x0404;
    static constexpr Enum kBack = 0x0405;
    static constexpr Enum kFrontAndBack = 0x0408;
    static constexpr Enum kClockwise = 0x0900;
    static constexpr Enum kCounterClockwise = 0x0901;

    static constexpr Enum kNever = 0x0200;
    static constexpr Enum kLess = 0x0201;
    static constexpr Enum kEqual = 0x0202;
    static constexpr Enum kLessEqual = 0x0203;
    static constexpr Enum kGreater = 0x0204;
    static constexpr Enum kNotEqual = 0x0205;
    static constexpr Enum kGreaterEqual = 0x0206;
    static constexpr Enum kAlways = 0x0207;

    static constexpr Enum kKeep = 0x1E00;
    static constexpr Enum kReplace = 0x1E01;
    static constexpr Enum kIncrement = 0x1E02;
    static constexpr Enum kDecrement = 0x1E03;
    static constexpr Enum kInvert = 0x150A;
    static constexpr Enum kIncrementWrap = 0x8507;
    static constexpr Enum kDecrementWrap = 0x8508;

    static constexpr Enum kSourceColor = 0x0300;
    static constexpr Enum kOneMinusSourceColor = 0x0301;
    static constexpr Enum kSourceAlpha = 0x0302;
    static constexpr Enum kOneMinusSourceAlpha = 0x0303;
    static constexpr Enum kDestinationAlpha = 0x0304;
    static constexpr Enum kOneMinusDestinationAlpha = 0x0305;
    static constexpr Enum kDestinationColor = 0x0306;
    static constexpr Enum kOneMinusDestinationColor = 0x0307;
    static constexpr Enum kSourceAlphaSaturate = 0x0308;
    static constexpr Enum kConstantColor = 0x8001;
    static constexpr Enum kOneMinusConstantColor = 0x8002;
    static constexpr Enum kFunctionAdd = 0x8006;
    static constexpr Enum kMinimum = 0x8007;
    static constexpr Enum kMaximum = 0x8008;
    static constexpr Enum kFunctionSubtract = 0x800A;
    static constexpr Enum kFunctionReverseSubtract = 0x800B;

    static constexpr Enum kArrayBuffer = 0x8892;
    static constexpr Enum kElementArrayBuffer = 0x8893;
    static constexpr Enum kPixelPackBuffer = 0x88EB;
    static constexpr Enum kPixelUnpackBuffer = 0x88EC;
    static constexpr Enum kCopyReadBuffer = 0x8F36;
    static constexpr Enum kCopyWriteBuffer = 0x8F37;
    static constexpr Enum kStreamDraw = 0x88E0;
    static constexpr Enum kStreamRead = 0x88E1;
    static constexpr Enum kStaticDraw = 0x88E4;
    static constexpr Enum kDynamicDraw = 0x88E8;
    static constexpr Bitfield kMapReadBit = 0x0001;
    static constexpr Bitfield kMapWriteBit = 0x0002;
    static constexpr Bitfield kMapInvalidateRangeBit = 0x0004;
    static constexpr Bitfield kMapUnsynchronizedBit = 0x0020;
    static constexpr Bitfield kMapPersistentBit = 0x0040;
    static constexpr Bitfield kMapCoherentBit = 0x0080;

    static constexpr Enum kTexture = 0x1702;
    static constexpr Enum kTexture2d = 0x0DE1;
    static constexpr Enum kTexture3d = 0x806F;
    static constexpr Enum kTexture2dArray = 0x8C1A;
    static constexpr Enum kTextureCubeMap = 0x8513;
    static constexpr Enum kTextureCubeMapPositiveX = 0x8515;
    static constexpr Enum kTexture0 = 0x84C0;
    static constexpr Enum kTextureMagFilter = 0x2800;
    static constexpr Enum kTextureMinFilter = 0x2801;
    static constexpr Enum kTextureWrapS = 0x2802;
    static constexpr Enum kTextureWrapT = 0x2803;
    static constexpr Enum kTextureWrapR = 0x8072;
    static constexpr Enum kTextureMinLod = 0x813A;
    static constexpr Enum kTextureMaxLod = 0x813B;
    static constexpr Enum kTextureMaxLevel = 0x813D;
    static constexpr Enum kTextureCompareMode = 0x884C;
    static constexpr Enum kTextureCompareFunc = 0x884D;
    static constexpr Enum kCompareRefToTexture = 0x884E;
    static constexpr Enum kTextureMaxAnisotropy = 0x84FE;
    static constexpr Enum kNearest = 0x2600;
    static constexpr Enum kLinear = 0x2601;
    static constexpr Enum kNearestMipmapNearest = 0x2700;
    static constexpr Enum kLinearMipmapNearest = 0x2701;
    static constexpr Enum kNearestMipmapLinear = 0x2702;
    static constexpr Enum kLinearMipmapLinear = 0x2703;
    static constexpr Enum kClampToEdge = 0x812F;
    static constexpr Enum kRepeat = 0x2901;
    static constexpr Enum kMirroredRepeat = 0x8370;
    static constexpr Enum kUnpackAlignment = 0x0CF5;
    static constexpr Enum kPackAlignment = 0x0D05;

    static constexpr Enum kRed = 0x1903;
    static constexpr Enum kRg = 0x8227;
    static constexpr Enum kRgba = 0x1908;
    static constexpr Enum kBgra = 0x80E1;
    static constexpr Enum kDepthComponent = 0x1902;
    static constexpr Enum kDepthStencil = 0x84F9;
    static constexpr Enum kR8 = 0x8229;
    static constexpr Enum kRg8 = 0x822B;
    static constexpr Enum kRgba8 = 0x8058;
    static constexpr Enum kSrgb8Alpha8 = 0x8C43;
    static constexpr Enum kRgb10A2 = 0x8059;
    static constexpr Enum kR16f = 0x822D;
    static constexpr Enum kRg16f = 0x822F;
    static constexpr Enum kRgba16f = 0x881A;
    static constexpr Enum kR32f = 0x822E;
    static constexpr Enum kRg32f = 0x8230;
    static constexpr Enum kRgba32f = 0x8814;
    static constexpr Enum kDepthComponent16 = 0x81A5;
    static constexpr Enum kDepthComponent32f = 0x8CAC;
    static constexpr Enum kDepth24Stencil8 = 0x88F0;
    static constexpr Enum kDepth32fStencil8 = 0x8CAD;

    static constexpr Enum kFramebuffer = 0x8D40;
    static constexpr Enum kReadFramebuffer = 0x8CA8;
    static constexpr Enum kDrawFramebuffer = 0x8CA9;
    static constexpr Enum kRenderbuffer = 0x8D41;
    static constexpr Enum kColorAttachment0 = 0x8CE0;
    static constexpr Enum kDepthAttachment = 0x8D00;
    static constexpr Enum kStencilAttachment = 0x8D20;
    static constexpr Enum kDepthStencilAttachment = 0x821A;
    static constexpr Enum kFramebufferComplete = 0x8CD5;
    static constexpr Enum kColor = 0x1800;
    static constexpr Enum kDepth = 0x1801;
    static constexpr Enum kStencil = 0x1802;
    static constexpr Bitfield kDepthBufferBit = 0x0100;
    static constexpr Bitfield kStencilBufferBit = 0x0400;
    static constexpr Bitfield kColorBufferBit = 0x4000;
    static constexpr Enum kSamples = 0x80A9;

    static constexpr Enum kFragmentShader = 0x8B30;
    static constexpr Enum kVertexShader = 0x8B31;
    static constexpr Enum kCompileStatus = 0x8B81;
    static constexpr Enum kLinkStatus = 0x8B82;
    static constexpr Enum kInfoLogLength = 0x8B84;

    static constexpr Enum kSyncGpuCommandsComplete = 0x9117;
    static constexpr Bitfield kSyncFlushCommandsBit = 0x0001;
    static constexpr Enum kSyncStatus = 0x9114;
    static constexpr Enum kSignaled = 0x9119;
    static constexpr Enum kAlreadySignaled = 0x911A;
    static constexpr Enum kTimeoutExpired = 0x911B;
    static constexpr Enum kConditionSatisfied = 0x911C;
    static constexpr Enum kWaitFailed = 0x911D;

    static constexpr Enum kTimeElapsed = 0x88BF;
    static constexpr Enum kQueryResult = 0x8866;
    static constexpr Enum kQueryResultAvailable = 0x8867;

    static constexpr Enum kDebugSourceApplication = 0x824A;
    static constexpr Enum kDebugTypeError = 0x824C;
    static constexpr Enum kDebugTypeDeprecatedBehavior = 0x824D;
    static constexpr Enum kDebugTypeUndefinedBehavior = 0x824E;
    static constexpr Enum kDebugTypePushGroup = 0x8269;
    static constexpr Enum kDebugTypePopGroup = 0x826A;
    static constexpr Enum kDebugSeverityNotification = 0x826B;
    static constexpr Enum kDontCare = 0x1100;
    static constexpr Enum kBuffer = 0x82E0;
    static constexpr Enum kProgram = 0x82E2;
    static constexpr Enum kSampler = 0x82E6;
    static constexpr Enum kVertexArray = 0x8074;
    static constexpr Enum kMaxLabelLength = 0x82E8;

    static constexpr Enum kGuiltyContextReset = 0x8253;
    static constexpr Enum kInnocentContextReset = 0x8254;
    static constexpr Enum kUnknownContextReset = 0x8255;

    // What the context offers past the version of its flavour.
    struct Features {
        int majorVersion = 0;
        int minorVersion = 0;
        bool bufferStorage = false;
        bool vertexAttribBinding = false;
        bool drawBuffersIndexed = false;
        bool timerQuery = false;
        bool debug = false;
        bool robustness = false;
        bool colorBufferFloat = false;
        bool colorBufferHalfFloat = false;
        bool floatBlend = false;
        bool textureFloatLinear = false;
        bool anisotropicFiltering = false;
    };

    // Loads every entry point of the flavour from the context, which must be current, and throws `std::runtime_error` when the context is older than the flavour needs or lacks one of them.
    void load(GlContext& context, Device::Api flavour);

    [[nodiscard]] Device::Api getFlavour() const noexcept {
        return api;
    }

    [[nodiscard]] const Features& getFeatures() const noexcept {
        return features;
    }

    [[nodiscard]] bool isDesktop() const noexcept {
        return api == Device::Api::OpenGl;
    }

    [[nodiscard]] bool isWeb() const noexcept {
        return api == Device::Api::WebGl2;
    }

    [[nodiscard]] std::string getString(Enum name) const;

    Enum (*getError)() = nullptr;
    void (*getIntegerv)(Enum name, Int* data) = nullptr;
    const std::uint8_t* (*getStringValue)(Enum name) = nullptr;
    const std::uint8_t* (*getStringi)(Enum name, Uint index) = nullptr;
    void (*enable)(Enum capability) = nullptr;
    void (*disable)(Enum capability) = nullptr;
    void (*viewport)(Int x, Int y, Sizei width, Sizei height) = nullptr;
    void (*scissor)(Int x, Int y, Sizei width, Sizei height) = nullptr;
    void (*colorMask)(Boolean red, Boolean green, Boolean blue, Boolean alpha) = nullptr;
    void (*depthMask)(Boolean flag) = nullptr;
    void (*depthFunc)(Enum function) = nullptr;
    void (*stencilFuncSeparate)(Enum face, Enum function, Int reference, Uint mask) = nullptr;
    void (*stencilOpSeparate)(Enum face, Enum fail, Enum depthFail, Enum pass) = nullptr;
    void (*stencilMaskSeparate)(Enum face, Uint mask) = nullptr;
    void (*blendFuncSeparate)(Enum sourceColor, Enum destinationColor, Enum sourceAlpha, Enum destinationAlpha) = nullptr;
    void (*blendEquationSeparate)(Enum color, Enum alpha) = nullptr;
    void (*blendColor)(Float red, Float green, Float blue, Float alpha) = nullptr;
    void (*cullFace)(Enum face) = nullptr;
    void (*frontFace)(Enum direction) = nullptr;
    void (*polygonOffset)(Float factor, Float units) = nullptr;
    void (*pixelStorei)(Enum name, Int value) = nullptr;
    void (*readPixels)(Int x, Int y, Sizei width, Sizei height, Enum format, Enum type, void* pixels) = nullptr;
    void (*readBuffer)(Enum source) = nullptr;
    void (*drawBuffers)(Sizei count, const Enum* buffers) = nullptr;
    void (*clearBufferfv)(Enum buffer, Int drawBuffer, const Float* value) = nullptr;
    void (*clearBufferiv)(Enum buffer, Int drawBuffer, const Int* value) = nullptr;
    void (*clearBufferfi)(Enum buffer, Int drawBuffer, Float depth, Int stencil) = nullptr;
    void (*flush)() = nullptr;

    void (*genBuffers)(Sizei count, Uint* buffers) = nullptr;
    void (*deleteBuffers)(Sizei count, const Uint* buffers) = nullptr;
    void (*bindBuffer)(Enum target, Uint buffer) = nullptr;
    void (*bufferData)(Enum target, SizeiPtr size, const void* data, Enum usage) = nullptr;
    void (*bufferSubData)(Enum target, IntPtr offset, SizeiPtr size, const void* data) = nullptr;
    void (*copyBufferSubData)(Enum readTarget, Enum writeTarget, IntPtr readOffset, IntPtr writeOffset, SizeiPtr size) = nullptr;
    void* (*mapBufferRange)(Enum target, IntPtr offset, SizeiPtr length, Bitfield access) = nullptr;
    Boolean (*unmapBuffer)(Enum target) = nullptr;
    void (*getBufferSubData)(Enum target, IntPtr offset, SizeiPtr size, void* data) = nullptr;
    void (*bufferStorage)(Enum target, SizeiPtr size, const void* data, Bitfield flags) = nullptr;

    void (*genTextures)(Sizei count, Uint* textures) = nullptr;
    void (*deleteTextures)(Sizei count, const Uint* textures) = nullptr;
    void (*bindTexture)(Enum target, Uint texture) = nullptr;
    void (*activeTexture)(Enum unit) = nullptr;
    void (*texParameteri)(Enum target, Enum name, Int value) = nullptr;
    void (*texStorage2D)(Enum target, Sizei levels, Enum format, Sizei width, Sizei height) = nullptr;
    void (*texStorage3D)(Enum target, Sizei levels, Enum format, Sizei width, Sizei height, Sizei depth) = nullptr;
    void (*texSubImage2D)(Enum target, Int level, Int x, Int y, Sizei width, Sizei height, Enum format, Enum type, const void* pixels) = nullptr;
    void (*texSubImage3D)(Enum target, Int level, Int x, Int y, Int z, Sizei width, Sizei height, Sizei depth, Enum format, Enum type, const void* pixels) = nullptr;

    void (*genSamplers)(Sizei count, Uint* samplers) = nullptr;
    void (*deleteSamplers)(Sizei count, const Uint* samplers) = nullptr;
    void (*bindSampler)(Uint unit, Uint sampler) = nullptr;
    void (*samplerParameteri)(Uint sampler, Enum name, Int value) = nullptr;
    void (*samplerParameterf)(Uint sampler, Enum name, Float value) = nullptr;

    void (*genRenderbuffers)(Sizei count, Uint* renderbuffers) = nullptr;
    void (*deleteRenderbuffers)(Sizei count, const Uint* renderbuffers) = nullptr;
    void (*bindRenderbuffer)(Enum target, Uint renderbuffer) = nullptr;
    void (*renderbufferStorageMultisample)(Enum target, Sizei samples, Enum format, Sizei width, Sizei height) = nullptr;
    void (*getInternalformativ)(Enum target, Enum format, Enum name, Sizei count, Int* values) = nullptr;

    void (*genFramebuffers)(Sizei count, Uint* framebuffers) = nullptr;
    void (*deleteFramebuffers)(Sizei count, const Uint* framebuffers) = nullptr;
    void (*bindFramebuffer)(Enum target, Uint framebuffer) = nullptr;
    void (*framebufferTexture2D)(Enum target, Enum attachment, Enum textureTarget, Uint texture, Int level) = nullptr;
    void (*framebufferTextureLayer)(Enum target, Enum attachment, Uint texture, Int level, Int layer) = nullptr;
    void (*framebufferRenderbuffer)(Enum target, Enum attachment, Enum renderbufferTarget, Uint renderbuffer) = nullptr;
    Enum (*checkFramebufferStatus)(Enum target) = nullptr;
    void (*blitFramebuffer)(Int sourceX0, Int sourceY0, Int sourceX1, Int sourceY1, Int destinationX0, Int destinationY0, Int destinationX1, Int destinationY1, Bitfield mask, Enum filter) = nullptr;
    void (*invalidateFramebuffer)(Enum target, Sizei count, const Enum* attachments) = nullptr;

    Uint (*createShader)(Enum type) = nullptr;
    void (*shaderSource)(Uint shader, Sizei count, const char* const* sources, const Int* lengths) = nullptr;
    void (*compileShader)(Uint shader) = nullptr;
    void (*getShaderiv)(Uint shader, Enum name, Int* value) = nullptr;
    void (*getShaderInfoLog)(Uint shader, Sizei size, Sizei* length, char* log) = nullptr;
    void (*deleteShader)(Uint shader) = nullptr;
    Uint (*createProgram)() = nullptr;
    void (*attachShader)(Uint program, Uint shader) = nullptr;
    void (*detachShader)(Uint program, Uint shader) = nullptr;
    void (*bindAttribLocation)(Uint program, Uint index, const char* name) = nullptr;
    void (*linkProgram)(Uint program) = nullptr;
    void (*getProgramiv)(Uint program, Enum name, Int* value) = nullptr;
    void (*getProgramInfoLog)(Uint program, Sizei size, Sizei* length, char* log) = nullptr;
    void (*deleteProgram)(Uint program) = nullptr;
    void (*useProgram)(Uint program) = nullptr;
    Int (*getUniformLocation)(Uint program, const char* name) = nullptr;
    void (*uniform1i)(Int location, Int value) = nullptr;
    void (*uniform4fv)(Int location, Sizei count, const Float* values) = nullptr;
    void (*uniform4iv)(Int location, Sizei count, const Int* values) = nullptr;

    void (*genVertexArrays)(Sizei count, Uint* arrays) = nullptr;
    void (*deleteVertexArrays)(Sizei count, const Uint* arrays) = nullptr;
    void (*bindVertexArray)(Uint array) = nullptr;
    void (*enableVertexAttribArray)(Uint index) = nullptr;
    void (*vertexAttribPointer)(Uint index, Int size, Enum type, Boolean normalized, Sizei stride, const void* offset) = nullptr;
    void (*vertexAttribIPointer)(Uint index, Int size, Enum type, Sizei stride, const void* offset) = nullptr;
    void (*vertexAttribDivisor)(Uint index, Uint divisor) = nullptr;
    void (*vertexAttribFormat)(Uint index, Int size, Enum type, Boolean normalized, Uint offset) = nullptr;
    void (*vertexAttribIFormat)(Uint index, Int size, Enum type, Uint offset) = nullptr;
    void (*vertexAttribBinding)(Uint index, Uint binding) = nullptr;
    void (*vertexBindingDivisor)(Uint binding, Uint divisor) = nullptr;
    void (*bindVertexBuffer)(Uint binding, Uint buffer, IntPtr offset, Sizei stride) = nullptr;

    void (*drawArraysInstanced)(Enum mode, Int first, Sizei count, Sizei instances) = nullptr;
    void (*drawElementsInstanced)(Enum mode, Sizei count, Enum type, const void* offset, Sizei instances) = nullptr;

    Sync (*fenceSync)(Enum condition, Bitfield flags) = nullptr;
    Enum (*clientWaitSync)(Sync sync, Bitfield flags, Uint64 timeout) = nullptr;
    void (*getSynciv)(Sync sync, Enum name, Sizei count, Sizei* length, Int* values) = nullptr;
    void (*deleteSync)(Sync sync) = nullptr;

    void (*genQueries)(Sizei count, Uint* queries) = nullptr;
    void (*deleteQueries)(Sizei count, const Uint* queries) = nullptr;
    void (*beginQuery)(Enum target, Uint query) = nullptr;
    void (*endQuery)(Enum target) = nullptr;
    void (*getQueryObjectuiv)(Uint query, Enum name, Uint* value) = nullptr;
    void (*getQueryObjectui64v)(Uint query, Enum name, Uint64* value) = nullptr;

    void (*enablei)(Enum capability, Uint index) = nullptr;
    void (*disablei)(Enum capability, Uint index) = nullptr;
    void (*blendFuncSeparatei)(Uint buffer, Enum sourceColor, Enum destinationColor, Enum sourceAlpha, Enum destinationAlpha) = nullptr;
    void (*blendEquationSeparatei)(Uint buffer, Enum color, Enum alpha) = nullptr;
    void (*colorMaski)(Uint buffer, Boolean red, Boolean green, Boolean blue, Boolean alpha) = nullptr;

    void (*debugMessageCallback)(DebugProc callback, const void* user) = nullptr;
    void (*debugMessageControl)(Enum source, Enum type, Enum severity, Sizei count, const Uint* ids, Boolean enabled) = nullptr;
    void (*objectLabel)(Enum identifier, Uint name, Sizei length, const char* label) = nullptr;
    void (*pushDebugGroup)(Enum source, Uint id, Sizei length, const char* message) = nullptr;
    void (*popDebugGroup)() = nullptr;

    Enum (*getGraphicsResetStatus)() = nullptr;

  private:
    // The suffixes of the names of the entry points of the optional features, which a context takes from the extension that brings each of them when its version lacks it.
    struct Suffixes {
        std::string_view bufferStorage;
        std::string_view drawBuffersIndexed;
        std::string_view timerQuery;
        std::string_view debug;
        std::string_view robustness;
    };

    // Points the entry point at the function of the name and the suffix, and leaves it null when the context knows no such function.
    template <typename Function> void bindSuffixed(GlContext& context, Function& function, std::string_view name, std::string_view suffix) {
        void* address = nullptr;
        bindNamed(context, address, name, suffix);
        function = reinterpret_cast<Function>(address);
    }

    template <typename Function> void require(GlContext& context, Function& function, std::string_view name) {
        bindSuffixed(context, function, name, "");
        if (function == nullptr) {
            throwMissing(name);
        }
    }

    static void bindNamed(GlContext& context, void*& address, std::string_view name, std::string_view suffix);
    [[noreturn]] void throwMissing(std::string_view name) const;
    [[nodiscard]] bool hasExtension(std::string_view name) const;
    void readVersion();
    void readFeatures();
    void loadCore(GlContext& context);
    void loadOptional(GlContext& context);

    Device::Api api = Device::Api::OpenGl;
    Features features;
    Suffixes suffixes;
    std::string extensions;
};

} // namespace haylen::gpu::opengl
