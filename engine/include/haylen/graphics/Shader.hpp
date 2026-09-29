#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::graphics {

struct ShaderResource;

// Shared handle to a custom shader: a .shader file that make.py shaders compiles from annotated GLSL for every backend, with the reflection of the uniforms and textures of its own. The engine creates the GPU program of the active backend the first time a draw needs it, and a reloaded file updates every handle.
class Shader final {
  public:
    enum class UniformType : std::uint8_t {
        Float,
        Vec2,
        Vec3,
        Vec4,
        Int,
        IVec2,
        IVec3,
        IVec4,
        Mat4,
    };

    // A member of a uniform block, with its byte offset inside the block and its number of elements.
    struct Uniform {
        std::string name;
        UniformType type = UniformType::Float;
        int count = 1;
        std::uint32_t offset = 0;
        std::size_t block = 0;
    };

    // A uniform block of the shader's own at its bind slot, which materials fill.
    struct Block {
        std::string name;
        int slot = 0;
        std::uint32_t size = 0;
    };

    struct TextureSlot {
        std::string name;
        int slot = 0;
    };

    Shader() = default;
    explicit Shader(std::shared_ptr<ShaderResource> value) noexcept : resource(std::move(value)) {}

    // Reads a .shader file and checks its programs against the binding limits of the GPU. Throws std::invalid_argument when the bytes are not a valid shader file.
    [[nodiscard]] static Shader parse(std::span<const std::uint8_t> bytes);

    // Resolves the GLSL names "float", "vec2", "vec3", "vec4", "int", "ivec2", "ivec3", "ivec4" and "mat4".
    [[nodiscard]] static std::optional<UniformType> uniformTypeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view uniformTypeName(UniformType type) noexcept;

    // Returns how many numbers one element of the type holds.
    [[nodiscard]] static int getComponentCount(UniformType type) noexcept;

    [[nodiscard]] bool isValid() const noexcept {
        return resource != nullptr;
    }
    [[nodiscard]] const std::string& getName() const;
    [[nodiscard]] const std::vector<Block>& getBlocks() const;
    [[nodiscard]] const std::vector<Uniform>& getUniforms() const;
    [[nodiscard]] const std::vector<TextureSlot>& getTextures() const;
    [[nodiscard]] const Uniform* findUniform(std::string_view name) const;
    [[nodiscard]] const TextureSlot* findTexture(std::string_view name) const;
    [[nodiscard]] bool hasProgram(std::string_view name) const;

    // Grows every time the shader takes new contents.
    [[nodiscard]] std::uint32_t getVersion() const;

    // Takes the programs and reflection of another shader, as when its file changes, so every handle to this one draws with them.
    void replace(const Shader& fresh);

    [[nodiscard]] const std::shared_ptr<ShaderResource>& getResource() const noexcept {
        return resource;
    }
    [[nodiscard]] bool operator==(const Shader& other) const noexcept {
        return resource == other.resource;
    }

  private:
    static const std::array<std::string_view, 9> kUniformTypeNames;

    [[nodiscard]] const ShaderResource& getChecked() const;

    std::shared_ptr<ShaderResource> resource;
};

} // namespace haylen::graphics
