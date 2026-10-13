#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/animation/SkeletonAnimation.hpp"
#include "haylen/2d/animation/SpriteAtlas.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::animation2d {

// The shared, immutable setup of a skeleton: a hierarchy of bones, the slots that hold one attachment each in draw order, the skins that give slots their attachments, the inverse kinematics and transform constraints, and the animations, with the sprite atlas whose frames the attachments show. Angles are in radians, lengths in pixels, and y grows downward.
class SkeletonData final {
  public:
    struct Bone {
        std::string name;
        std::optional<std::size_t> parent;
        math::Vec2 position{};
        float rotation = 0.0F;
        math::Vec2 scale{1.0F, 1.0F};
        float length = 0.0F;
    };

    struct Slot {
        std::string name;
        std::size_t bone = 0;
        math::Color color = math::Color::white();
        std::string attachment;
        graphics::BlendMode::Type blend = graphics::BlendMode::Type::Alpha;
    };

    // What a slot shows: a region of the atlas placed on its bone, a mesh of a region whose vertices follow one bone or several by weight, a point such as a muzzle, or an outline for hit tests.
    struct Attachment {
        enum class Kind : std::uint8_t {
            Region,
            Mesh,
            Point,
            BoundingBox,
        };

        // One bone that moves a vertex of a weighted mesh, with the position of the vertex in the space of that bone.
        struct Influence {
            std::size_t bone = 0;
            math::Vec2 position{};
            float weight = 1.0F;
        };

        Kind kind = Kind::Region;
        std::string name;

        // The frame of the atlas a region or a mesh shows.
        std::string frame;
        math::Vec2 position{};
        float rotation = 0.0F;
        math::Vec2 scale{1.0F, 1.0F};
        math::Vec2 size{};
        math::Color color = math::Color::white();

        // The vertices of a mesh or an outline in the space of its bone, the texture coordinates of a mesh from 0 to 1 over its frame, its triangles, and the influences of each vertex of a weighted mesh, which starts at `influenceStarts[vertex]`.
        std::vector<math::Vec2> vertices;
        std::vector<math::Vec2> uvs;
        std::vector<std::uint32_t> triangles;
        std::vector<Influence> influences;
        std::vector<std::size_t> influenceStarts;

        [[nodiscard]] bool isWeighted() const noexcept {
            return !influenceStarts.empty();
        }
        [[nodiscard]] std::size_t getVertexCount() const noexcept {
            return isWeighted() ? influenceStarts.size() - 1 : vertices.size();
        }
    };

    // The attachments of a skin by slot and by name.
    struct Skin {
        std::string name;
        std::vector<std::map<std::string, Attachment, std::less<>>> slots;
    };

    // Bends one bone, or a parent and its child, so the end of the chain reaches the target bone, or the point a skeleton gives it, with the bend direction of the elbow and a mix from 0 to 1.
    struct IkConstraint {
        std::string name;
        std::vector<std::size_t> bones;
        std::size_t target = 0;
        bool bendPositive = true;
        float mix = 1.0F;
    };

    // Moves bones toward the world rotation and position of a target bone, by mixes from 0 to 1, offset by a rotation and a position.
    struct TransformConstraint {
        std::string name;
        std::vector<std::size_t> bones;
        std::size_t target = 0;
        float rotateMix = 1.0F;
        float translateMix = 1.0F;
        float rotation = 0.0F;
        math::Vec2 offset{};
    };

    // Reads the JSON of a `.skeleton` file over the atlas its attachments show, and throws `std::invalid_argument` with a sentence that names what is wrong, such as an unknown bone or a malformed timeline.
    [[nodiscard]] static SkeletonData parse(const core::Json& document, std::shared_ptr<const SpriteAtlas> frames);

    // Returns the path of the atlas a `.skeleton` file names, relative to the file.
    [[nodiscard]] static std::string atlasPath(const core::Json& document);

    [[nodiscard]] const std::vector<Bone>& getBones() const noexcept {
        return bones;
    }
    [[nodiscard]] const std::vector<Slot>& getSlots() const noexcept {
        return slots;
    }
    [[nodiscard]] const std::vector<Skin>& getSkins() const noexcept {
        return skins;
    }
    [[nodiscard]] const std::vector<IkConstraint>& getIkConstraints() const noexcept {
        return ikConstraints;
    }
    [[nodiscard]] const std::vector<TransformConstraint>& getTransformConstraints() const noexcept {
        return transformConstraints;
    }
    [[nodiscard]] const std::vector<SkeletonAnimation>& getAnimations() const noexcept {
        return animations;
    }
    [[nodiscard]] const std::vector<std::string>& getEvents() const noexcept {
        return events;
    }
    [[nodiscard]] const SpriteAtlas& getAtlas() const noexcept {
        return *atlas;
    }

    [[nodiscard]] std::optional<std::size_t> findBone(std::string_view name) const noexcept;
    [[nodiscard]] std::optional<std::size_t> findSlot(std::string_view name) const noexcept;
    [[nodiscard]] std::optional<std::size_t> findSkin(std::string_view name) const noexcept;
    [[nodiscard]] std::optional<std::size_t> findIkConstraint(std::string_view name) const noexcept;
    [[nodiscard]] const SkeletonAnimation* findAnimation(std::string_view name) const noexcept;

    // Returns the attachment of a slot with a name in a skin, falling back to the default skin, or nothing.
    [[nodiscard]] const Attachment* findAttachment(std::size_t skin, std::size_t slot, std::string_view name) const noexcept;

  private:
    // Reads the parts of the document in the order their references need.
    class Reader;

    std::shared_ptr<const SpriteAtlas> atlas;
    std::vector<Bone> bones;
    std::vector<Slot> slots;
    std::vector<Skin> skins;
    std::vector<IkConstraint> ikConstraints;
    std::vector<TransformConstraint> transformConstraints;
    std::vector<SkeletonAnimation> animations;
    std::vector<std::string> events;
};

} // namespace haylen::animation2d
