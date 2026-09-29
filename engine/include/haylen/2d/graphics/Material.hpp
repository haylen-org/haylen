#pragma once

#include <array>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "haylen/graphics/Shader.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Transform2D.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

struct MaterialResource;

// Shared handle to a custom shader with values for its uniforms and textures. Draws take a material through their draw order, which replaces how their pixels are shaded for sprites, batches, primitives, meshes and text in every canvas, and post-processing chains of world canvases run materials over the whole image. A draw keeps the values its material had when it was made.
class Material final {
  public:
    // The programs a material shader holds, one per kind of draw and each also for lit canvases, which make.py shaders compiles from every source.
    static const std::array<std::string_view, 6> kPrograms;

    Material() = default;

    // Throws std::invalid_argument when the shader is empty or lacks one of the programs.
    explicit Material(graphics::Shader shader);

    [[nodiscard]] bool isValid() const noexcept {
        return resource != nullptr;
    }
    [[nodiscard]] const graphics::Shader& getShader() const;

    // Sets a uniform by name: a number fills a float or int, a vector fills a vec2, a color fills a vec3 with its red, green and blue or a vec4 with all four, and a transform fills a mat4. Throws std::invalid_argument when the shader has no such uniform or the value does not fit its type.
    void set(std::string_view name, float value);
    void set(std::string_view name, math::Vec2 value);
    void set(std::string_view name, math::Color value);
    void set(std::string_view name, const math::Transform2D& value);

    // Sets every number of a uniform at once, such as the 16 numbers of a mat4 in column order or all the elements of an array, and throws std::invalid_argument when the count does not match.
    void set(std::string_view name, std::span<const float> values);
    void setTexture(std::string_view name, graphics::Texture texture);

    // Returns the numbers of a uniform, which are zero until it is set.
    [[nodiscard]] std::vector<float> get(std::string_view name) const;
    [[nodiscard]] graphics::Texture getTexture(std::string_view name) const;

    [[nodiscard]] const std::shared_ptr<MaterialResource>& getResource() const noexcept {
        return resource;
    }
    [[nodiscard]] bool operator==(const Material& other) const noexcept {
        return resource == other.resource;
    }

  private:
    [[nodiscard]] MaterialResource& getChecked() const;
    [[nodiscard]] const graphics::Shader::Uniform& findUniform(std::string_view name) const;

    std::shared_ptr<MaterialResource> resource;
};

} // namespace haylen::graphics2d
