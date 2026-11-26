#include "3d/gltf/LinkValidator.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

#include "3d/gltf/FormatNames.hpp"

namespace haylen::gltf {

using Problem = ReadError::Problem;
using Component = Accessor::ComponentType;
using Type = Accessor::Type;

LinkValidator::LinkValidator(Document& target, const Diagnostics& documentDiagnostics) : document(target), diagnostics(documentDiagnostics), viewUses(target.bufferViews.size(), ViewUse::None), attributeViewAccessors(target.bufferViews.size()) {}

void LinkValidator::validate(Document& document, const Diagnostics& diagnostics) {
    LinkValidator validator(document, diagnostics);
    validator.validateMeshes();
    validator.validateNodes();
    validator.validateSkins();
    validator.validateAnimations();
    validator.validateTextures();
}

void LinkValidator::validateMeshes() {
    for (std::size_t meshIndex = 0; meshIndex < document.meshes.size(); ++meshIndex) {
        const Mesh& mesh = document.meshes[meshIndex];
        const std::string owner = Diagnostics::item("mesh", meshIndex);
        std::optional<std::size_t> targets;
        for (std::size_t index = 0; index < mesh.primitives.size(); ++index) {
            const Mesh::Primitive& primitive = mesh.primitives[index];
            validatePrimitive(primitive, Diagnostics::item("primitive", index, owner));
            if (primitive.targets.empty()) {
                continue;
            }

            if (targets && *targets != primitive.targets.size()) {
                diagnostics.invalid(Problem::MorphTargets, "the primitives of " + owner + " hold different numbers of morph targets");
            }

            targets = primitive.targets.size();
        }

        if (!mesh.weights.empty() && mesh.weights.size() != targets.value_or(0)) {
            diagnostics.invalid(Problem::WeightsCount, owner + " gives " + std::to_string(mesh.weights.size()) + " default weights for " + std::to_string(targets.value_or(0)) + " morph targets");
        }
    }
}

void LinkValidator::validatePrimitive(const Mesh::Primitive& primitive, const std::string& owner) {
    validateAttributes(primitive.attributes, owner, false);
    const std::uint64_t vertices = document.accessors[primitive.attributes.front().accessor].count;
    for (const Mesh::Attribute& attribute : primitive.attributes) {
        if (document.accessors[attribute.accessor].count != vertices) {
            diagnostics.invalid(Problem::AttributeCount, "the attributes of " + owner + " hold different numbers of vertices");
        }
    }

    for (std::size_t target = 0; target < primitive.targets.size(); ++target) {
        const std::string targetOwner = Diagnostics::item("morph target", target, owner);
        validateAttributes(primitive.targets[target], targetOwner, true);
        for (const Mesh::Attribute& attribute : primitive.targets[target]) {
            if (!primitive.findAttribute(attribute.name)) {
                diagnostics.invalid(Problem::MorphTargets, targetOwner + " moves the attribute \"" + attribute.name + "\", which the primitive lacks");
            }

            if (document.accessors[attribute.accessor].count != vertices) {
                diagnostics.invalid(Problem::AttributeCount, targetOwner + " holds a number of vertices other than the primitive's");
            }
        }
    }

    std::uint64_t count = vertices;
    if (primitive.indices) {
        const std::string indicesOwner = "the indices of " + owner;
        requireFormat(*primitive.indices, {{Type::Scalar, Component::UnsignedByte, false}, {Type::Scalar, Component::UnsignedShort, false}, {Type::Scalar, Component::UnsignedInt, false}}, indicesOwner);
        const Accessor& indices = document.accessors[*primitive.indices];
        if (indices.bufferView) {
            const BufferView& view = document.bufferViews[*indices.bufferView];
            if (view.byteStride) {
                diagnostics.invalid(Problem::InvalidByteStride, "buffer view " + std::to_string(*indices.bufferView) + " holds " + indicesOwner + " and so takes no stride");
            }

            if (view.target && *view.target != BufferView::Target::ElementArrayBuffer) {
                diagnostics.invalid(Problem::UsageConflict, "buffer view " + std::to_string(*indices.bufferView) + " holds " + indicesOwner + " but names vertices as its target");
            }
        }

        useAccessorView(*primitive.indices, ViewUse::Indices, indicesOwner);
        count = indices.count;
    }

    const bool fits = joins(primitive.mode, count);
    if (!fits) {
        diagnostics.invalid(Problem::VertexCount, owner + " draws " + std::to_string(count) + " vertices, which its mode cannot join");
    }

    if (!primitive.material) {
        return;
    }

    // Every set of texture coordinates that the material reads must be an attribute of the primitive.
    const Material& material = document.materials[*primitive.material];
    for (const std::optional<Material::TextureInfo>* texture : {&material.baseColorTexture, &material.metallicRoughnessTexture, &material.normalTexture, &material.occlusionTexture, &material.emissiveTexture}) {
        if (texture->has_value() && !primitive.findAttribute("TEXCOORD_" + std::to_string((*texture)->texCoord))) {
            diagnostics.invalid(Problem::InvalidAttribute, owner + " lacks the attribute \"TEXCOORD_" + std::to_string((*texture)->texCoord) + "\" that material " + std::to_string(*primitive.material) + " reads a texture at");
        }
    }
}

bool LinkValidator::joins(Mesh::Primitive::Mode mode, std::uint64_t count) noexcept {
    using Mode = Mesh::Primitive::Mode;
    switch (mode) {
    case Mode::Points:
        return count >= 1;
    case Mode::Lines:
        return count % 2 == 0;
    case Mode::LineLoop:
    case Mode::LineStrip:
        return count >= 2;
    case Mode::Triangles:
        return count % 3 == 0;
    case Mode::TriangleStrip:
    case Mode::TriangleFan:
        return count >= 3;
    }
    return false;
}

std::optional<LinkValidator::Semantic> LinkValidator::parseSemantic(std::string_view name, const std::string& owner, bool morph) const {
    if (name.starts_with('_')) {
        return std::nullopt;
    }

    if (name == "POSITION" || name == "NORMAL" || name == "TANGENT") {
        return Semantic{.base = std::string(name)};
    }

    const std::size_t underscore = name.find('_');
    const std::string_view base = name.substr(0, underscore);
    const bool indexedBase = base == "TEXCOORD" || base == "COLOR" || (!morph && (base == "JOINTS" || base == "WEIGHTS"));
    const std::string_view digits = underscore == std::string_view::npos ? std::string_view() : name.substr(underscore + 1);
    const bool validIndex = !digits.empty() && digits.size() <= 9 && std::ranges::all_of(digits, [](char character) { return character >= '0' && character <= '9'; }) && (digits.size() == 1 || digits.front() != '0');
    if (!indexedBase || !validIndex) {
        diagnostics.invalid(Problem::InvalidAttribute, owner + " has the attribute \"" + std::string(name) + "\", which glTF 2.0 does not define and which does not start with an underscore");
    }

    return Semantic{.base = std::string(base), .index = static_cast<std::uint32_t>(std::stoul(std::string(digits))), .indexed = true};
}

void LinkValidator::validateAttributes(const std::vector<Mesh::Attribute>& attributes, const std::string& owner, bool morph) {
    constexpr Format kVec2Float{Type::Vec2, Component::Float, false};
    std::map<std::string, std::set<std::uint32_t>> indexed;
    for (const Mesh::Attribute& attribute : attributes) {
        const std::string attributeOwner = "the attribute \"" + attribute.name + "\" of " + owner;
        const Accessor& accessor = document.accessors[attribute.accessor];
        const std::optional<Semantic> semantic = parseSemantic(attribute.name, owner, morph);
        if (!semantic) {
            if (accessor.componentType == Component::UnsignedInt) {
                diagnostics.invalid(Problem::AccessorFormat, attributeOwner + " holds unsigned integers, which no attribute takes");
            }
        } else if (semantic->base == "POSITION" || semantic->base == "NORMAL" || (semantic->base == "TANGENT" && morph)) {
            requireFormat(attribute.accessor, {{Type::Vec3, Component::Float, false}}, attributeOwner);
        } else if (semantic->base == "TANGENT") {
            requireFormat(attribute.accessor, {{Type::Vec4, Component::Float, false}}, attributeOwner);
        } else if (semantic->base == "TEXCOORD" && morph) {
            requireFormat(attribute.accessor, {kVec2Float, {Type::Vec2, Component::Byte, true}, {Type::Vec2, Component::Short, true}, {Type::Vec2, Component::UnsignedByte, true}, {Type::Vec2, Component::UnsignedShort, true}}, attributeOwner);
        } else if (semantic->base == "TEXCOORD") {
            requireFormat(attribute.accessor, {kVec2Float, {Type::Vec2, Component::UnsignedByte, true}, {Type::Vec2, Component::UnsignedShort, true}}, attributeOwner);
        } else if (semantic->base == "COLOR" && morph) {
            requireFormat(attribute.accessor, {{Type::Vec3, Component::Float, false}, {Type::Vec3, Component::Byte, true}, {Type::Vec3, Component::Short, true}, {Type::Vec3, Component::UnsignedByte, true}, {Type::Vec3, Component::UnsignedShort, true}, {Type::Vec4, Component::Float, false}, {Type::Vec4, Component::Byte, true}, {Type::Vec4, Component::Short, true}, {Type::Vec4, Component::UnsignedByte, true}, {Type::Vec4, Component::UnsignedShort, true}}, attributeOwner);
        } else if (semantic->base == "COLOR") {
            requireFormat(attribute.accessor, {{Type::Vec3, Component::Float, false}, {Type::Vec3, Component::UnsignedByte, true}, {Type::Vec3, Component::UnsignedShort, true}, {Type::Vec4, Component::Float, false}, {Type::Vec4, Component::UnsignedByte, true}, {Type::Vec4, Component::UnsignedShort, true}}, attributeOwner);
        } else if (semantic->base == "JOINTS") {
            requireFormat(attribute.accessor, {{Type::Vec4, Component::UnsignedByte, false}, {Type::Vec4, Component::UnsignedShort, false}}, attributeOwner);
        } else {
            requireFormat(attribute.accessor, {{Type::Vec4, Component::Float, false}, {Type::Vec4, Component::UnsignedByte, true}, {Type::Vec4, Component::UnsignedShort, true}}, attributeOwner);
        }

        if (semantic && semantic->indexed) {
            indexed[semantic->base].insert(semantic->index);
        }

        if (semantic && semantic->base == "POSITION" && (accessor.min.empty() || accessor.max.empty())) {
            diagnostics.invalid(Problem::MissingProperty, "accessor " + std::to_string(attribute.accessor) + ", the positions of " + owner + ", lacks the bounds \"min\" and \"max\" that positions need");
        }

        // Vertex data lies on boundaries of four bytes, and a buffer view that several attributes share names the stride between its vertices.
        if (accessor.bufferView) {
            const std::size_t viewIndex = *accessor.bufferView;
            const BufferView& view = document.bufferViews[viewIndex];
            if (accessor.byteOffset % 4 != 0 || (accessor.getElementSize() % 4 != 0 && !view.byteStride)) {
                diagnostics.invalid(Problem::Misaligned, attributeOwner + " does not start each vertex on a boundary of four bytes");
            }

            if (view.target && *view.target != BufferView::Target::ArrayBuffer) {
                diagnostics.invalid(Problem::UsageConflict, "buffer view " + std::to_string(viewIndex) + " holds " + attributeOwner + " but names indices as its target");
            }

            std::optional<std::size_t>& first = attributeViewAccessors[viewIndex];
            if (first && *first != attribute.accessor && !view.byteStride) {
                diagnostics.invalid(Problem::InvalidByteStride, "buffer view " + std::to_string(viewIndex) + " holds several attributes without a stride between its vertices");
            }

            first = attribute.accessor;
        }

        useAccessorView(attribute.accessor, ViewUse::Vertices, attributeOwner);
    }

    for (const auto& [base, indices] : indexed) {
        if (*indices.rbegin() + 1 != indices.size()) {
            diagnostics.invalid(Problem::InvalidAttribute, "the attributes \"" + base + "_<n>\" of " + owner + " skip a number");
        }
    }

    if (!morph && indexed["JOINTS"] != indexed["WEIGHTS"]) {
        diagnostics.invalid(Problem::InvalidAttribute, "the attributes \"JOINTS_<n>\" and \"WEIGHTS_<n>\" of " + owner + " do not come in pairs");
    }
}

void LinkValidator::validateNodes() {
    std::vector<Node>& nodes = document.nodes;
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        for (const std::size_t child : nodes[index].children) {
            if (nodes[child].parent) {
                diagnostics.invalid(Problem::NodeParent, "node " + std::to_string(child) + " is a child of both node " + std::to_string(*nodes[child].parent) + " and node " + std::to_string(index));
            }

            nodes[child].parent = index;
        }
    }

    // Each node has at most one parent, so walking up from every node either reaches a root or comes back to a node of the same walk.
    std::vector<std::uint8_t> state(nodes.size(), 0);
    for (std::size_t start = 0; start < nodes.size(); ++start) {
        std::size_t node = start;
        std::vector<std::size_t> walked;
        while (state[node] == 0) {
            state[node] = 1;
            walked.push_back(node);
            if (!nodes[node].parent) {
                break;
            }

            node = *nodes[node].parent;
        }

        if (state[node] == 1 && nodes[node].parent && std::ranges::find(walked, node) != walked.end()) {
            diagnostics.invalid(Problem::NodeCycle, "node " + std::to_string(node) + " is its own ancestor");
        }

        for (const std::size_t visited : walked) {
            state[visited] = 2;
        }
    }

    for (std::size_t sceneIndex = 0; sceneIndex < document.scenes.size(); ++sceneIndex) {
        for (const std::size_t node : document.scenes[sceneIndex].nodes) {
            if (nodes[node].parent) {
                diagnostics.invalid(Problem::SceneRoot, "scene " + std::to_string(sceneIndex) + " lists node " + std::to_string(node) + " as a root, while node " + std::to_string(*nodes[node].parent) + " is its parent");
            }
        }
    }

    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const Node& node = nodes[index];
        if (!node.mesh) {
            continue;
        }

        const Mesh& mesh = document.meshes[*node.mesh];
        if (!node.weights.empty() && node.weights.size() != mesh.getTargetCount()) {
            diagnostics.invalid(Problem::WeightsCount, "node " + std::to_string(index) + " gives " + std::to_string(node.weights.size()) + " weights for the " + std::to_string(mesh.getTargetCount()) + " morph targets of mesh " + std::to_string(*node.mesh));
        }

        if (!node.skin) {
            continue;
        }

        for (const Mesh::Primitive& primitive : mesh.primitives) {
            if (!primitive.findAttribute("JOINTS_0") || !primitive.findAttribute("WEIGHTS_0")) {
                diagnostics.invalid(Problem::SkinMesh, "node " + std::to_string(index) + " bends mesh " + std::to_string(*node.mesh) + " with a skin, while a primitive of the mesh has no joints and weights");
            }
        }
    }
}

std::size_t LinkValidator::findRoot(std::size_t node) const noexcept {
    while (document.nodes[node].parent) {
        node = *document.nodes[node].parent;
    }

    return node;
}

bool LinkValidator::isAncestor(std::size_t ancestor, std::size_t node) const noexcept {
    for (std::optional<std::size_t> current = node; current; current = document.nodes[*current].parent) {
        if (*current == ancestor) {
            return true;
        }
    }

    return false;
}

void LinkValidator::validateSkins() {
    for (std::size_t index = 0; index < document.skins.size(); ++index) {
        const Skin& skin = document.skins[index];
        const std::string owner = Diagnostics::item("skin", index);
        if (skin.inverseBindMatrices) {
            const std::string matricesOwner = "the inverse bind matrices of " + owner;
            requireFormat(*skin.inverseBindMatrices, {{Type::Mat4, Component::Float, false}}, matricesOwner);
            requireNoStrideOrTarget(*skin.inverseBindMatrices, matricesOwner, true);
            if (document.accessors[*skin.inverseBindMatrices].count < skin.joints.size()) {
                diagnostics.invalid(Problem::AccessorFormat, matricesOwner + " hold fewer matrices than its " + std::to_string(skin.joints.size()) + " joints");
            }

            useAccessorView(*skin.inverseBindMatrices, ViewUse::InverseBindMatrices, matricesOwner);
        }

        const std::size_t root = findRoot(skin.joints.front());
        for (const std::size_t joint : skin.joints) {
            if (findRoot(joint) != root) {
                diagnostics.invalid(Problem::SkinHierarchy, "the joints of " + owner + " lie in different hierarchies of nodes");
            }

            if (skin.skeleton && !isAncestor(*skin.skeleton, joint)) {
                diagnostics.invalid(Problem::SkinHierarchy, "the skeleton of " + owner + ", node " + std::to_string(*skin.skeleton) + ", is not an ancestor of its joint node " + std::to_string(joint));
            }
        }
    }

    // A skinned node of a scene bends with joints of the same scene.
    for (std::size_t index = 0; index < document.nodes.size(); ++index) {
        const Node& node = document.nodes[index];
        if (!node.skin) {
            continue;
        }

        const std::size_t nodeRoot = findRoot(index);
        const std::size_t jointRoot = findRoot(document.skins[*node.skin].joints.front());
        for (std::size_t sceneIndex = 0; sceneIndex < document.scenes.size(); ++sceneIndex) {
            const std::vector<std::size_t>& roots = document.scenes[sceneIndex].nodes;
            if (std::ranges::find(roots, nodeRoot) != roots.end() && std::ranges::find(roots, jointRoot) == roots.end()) {
                diagnostics.invalid(Problem::SkinHierarchy, "node " + std::to_string(index) + " of scene " + std::to_string(sceneIndex) + " bends with joints that the scene does not hold");
            }
        }
    }
}

void LinkValidator::validateAnimations() {
    for (std::size_t index = 0; index < document.animations.size(); ++index) {
        const Animation& animation = document.animations[index];
        const std::string owner = Diagnostics::item("animation", index);
        for (std::size_t samplerIndex = 0; samplerIndex < animation.samplers.size(); ++samplerIndex) {
            const Animation::Sampler& sampler = animation.samplers[samplerIndex];
            const std::string samplerOwner = Diagnostics::item("sampler", samplerIndex, owner);
            const std::string inputOwner = "the times of " + samplerOwner;
            requireFormat(sampler.input, {{Type::Scalar, Component::Float, false}}, inputOwner);
            requireNoStrideOrTarget(sampler.input, inputOwner, false);
            requireNoStrideOrTarget(sampler.output, "the values of " + samplerOwner, false);
            const Accessor& input = document.accessors[sampler.input];
            if (input.min.empty() || input.max.empty()) {
                diagnostics.invalid(Problem::MissingProperty, inputOwner + " lack the bounds \"min\" and \"max\" that the times of an animation need");
            }

            if (sampler.interpolation == Animation::Interpolation::CubicSpline && input.count < 2) {
                diagnostics.invalid(Problem::InvalidTimes, samplerOwner + " is a cubic spline with fewer than two keyframes");
            }
        }

        std::set<std::pair<std::size_t, Animation::Path>> targets;
        for (std::size_t channelIndex = 0; channelIndex < animation.channels.size(); ++channelIndex) {
            const Animation::Channel& channel = animation.channels[channelIndex];
            const std::string channelOwner = Diagnostics::item("channel", channelIndex, owner);
            if (!channel.node) {
                continue;
            }

            const Node& node = document.nodes[*channel.node];
            if (!targets.insert({*channel.node, channel.path}).second) {
                diagnostics.invalid(Problem::AnimationTarget, owner + " animates the same property of node " + std::to_string(*channel.node) + " in two channels");
            }

            if (node.matrix) {
                diagnostics.invalid(Problem::AnimationTarget, channelOwner + " animates node " + std::to_string(*channel.node) + ", which gives its transform as a matrix");
            }

            std::uint64_t values = 1;
            const Animation::Sampler& sampler = animation.samplers[channel.sampler];
            const std::string outputOwner = "the values of " + Diagnostics::item("sampler", channel.sampler, owner);
            switch (channel.path) {
            case Animation::Path::Translation:
            case Animation::Path::Scale:
                requireFormat(sampler.output, {{Type::Vec3, Component::Float, false}}, outputOwner);
                break;
            case Animation::Path::Rotation:
                requireFormat(sampler.output, {{Type::Vec4, Component::Float, false}, {Type::Vec4, Component::Byte, true}, {Type::Vec4, Component::UnsignedByte, true}, {Type::Vec4, Component::Short, true}, {Type::Vec4, Component::UnsignedShort, true}}, outputOwner);
                break;
            case Animation::Path::Weights:
                if (!node.mesh || document.meshes[*node.mesh].getTargetCount() == 0) {
                    diagnostics.invalid(Problem::AnimationTarget, channelOwner + " animates the morph weights of node " + std::to_string(*channel.node) + ", whose mesh has no morph targets");
                }

                requireFormat(sampler.output, {{Type::Scalar, Component::Float, false}, {Type::Scalar, Component::Byte, true}, {Type::Scalar, Component::UnsignedByte, true}, {Type::Scalar, Component::Short, true}, {Type::Scalar, Component::UnsignedShort, true}}, outputOwner);
                values = document.meshes[*node.mesh].getTargetCount();
                break;
            }

            const std::uint64_t keyframes = document.accessors[sampler.input].count;
            const std::uint64_t perKeyframe = sampler.interpolation == Animation::Interpolation::CubicSpline ? 3 * values : values;
            if (document.accessors[sampler.output].count != keyframes * perKeyframe) {
                diagnostics.invalid(Problem::AccessorFormat, outputOwner + " hold " + std::to_string(document.accessors[sampler.output].count) + " elements, while its " + std::to_string(keyframes) + " keyframes need " + std::to_string(keyframes * perKeyframe));
            }
        }
    }
}

void LinkValidator::validateTextures() {
    for (std::size_t index = 0; index < document.textures.size(); ++index) {
        const Texture& texture = document.textures[index];
        const std::string owner = Diagnostics::item("texture", index);
        if (texture.source) {
            const std::string& mimeType = document.images[*texture.source].mimeType;
            if (!mimeType.empty() && mimeType != "image/png" && mimeType != "image/jpeg") {
                diagnostics.invalid(Problem::InvalidImage, owner + " takes image " + std::to_string(*texture.source) + " of the media type \"" + mimeType + "\" as its source, which takes only PNG and JPEG images");
            }
        }

        if (texture.extensions.is_object() && texture.extensions.contains("EXT_texture_webp")) {
            const std::size_t image = texture.extensions.at("EXT_texture_webp").at("source").get<std::size_t>();
            const std::string& mimeType = document.images[image].mimeType;
            if (!mimeType.empty() && mimeType != "image/webp") {
                diagnostics.invalid(Problem::InvalidImage, owner + " takes image " + std::to_string(image) + " of the media type \"" + mimeType + "\" as its WebP image");
            }
        }
    }

    for (std::size_t index = 0; index < document.images.size(); ++index) {
        const Image& image = document.images[index];
        if (!image.bufferView) {
            continue;
        }

        const BufferView& view = document.bufferViews[*image.bufferView];
        if (view.byteStride || view.target) {
            diagnostics.invalid(Problem::InvalidByteStride, "buffer view " + std::to_string(*image.bufferView) + " holds image " + std::to_string(index) + " and so takes no stride and no target");
        }

        useView(*image.bufferView, ViewUse::Image, Diagnostics::item("image", index));
    }

    // A buffer view that an image takes as a whole is not divided by accessors.
    for (std::size_t index = 0; index < document.accessors.size(); ++index) {
        const Accessor& accessor = document.accessors[index];
        if (accessor.bufferView && viewUses[*accessor.bufferView] == ViewUse::Image) {
            diagnostics.invalid(Problem::UsageConflict, "buffer view " + std::to_string(*accessor.bufferView) + " holds an image and also the elements of accessor " + std::to_string(index));
        }
    }
}

void LinkValidator::useView(std::size_t view, ViewUse use, const std::string& owner) {
    ViewUse& current = viewUses[view];
    if (current != ViewUse::None && current != use) {
        diagnostics.invalid(Problem::UsageConflict, "buffer view " + std::to_string(view) + " holds " + owner + " besides data of another kind");
    }

    current = use;
}

void LinkValidator::useAccessorView(std::size_t accessor, ViewUse use, const std::string& owner) {
    if (const std::optional<std::size_t> view = document.accessors[accessor].bufferView) {
        useView(*view, use, owner);
    }
}

void LinkValidator::requireFormat(std::size_t accessor, std::initializer_list<Format> formats, const std::string& owner) const {
    const Accessor& found = document.accessors[accessor];
    for (const Format& format : formats) {
        if (format.type == found.type && format.componentType == found.componentType && format.normalized == found.normalized) {
            return;
        }
    }

    std::string accepted;
    for (const Format& format : formats) {
        accepted += (accepted.empty() ? "" : ", ") + FormatNames::describeFormat(format.type, format.componentType, format.normalized);
    }

    diagnostics.invalid(Problem::AccessorFormat, "accessor " + std::to_string(accessor) + ", " + owner + ", holds " + FormatNames::describeFormat(found.type, found.componentType, found.normalized) + ", while it takes " + accepted);
}

void LinkValidator::requireNoStrideOrTarget(std::size_t accessor, const std::string& owner, bool allowTarget) const {
    const std::optional<std::size_t> view = document.accessors[accessor].bufferView;
    if (!view) {
        return;
    }

    const BufferView& bufferView = document.bufferViews[*view];
    if (bufferView.byteStride || (!allowTarget && bufferView.target)) {
        diagnostics.invalid(Problem::InvalidByteStride, "buffer view " + std::to_string(*view) + " holds " + owner + " and so takes no stride" + (allowTarget ? "" : " and no target"));
    }
}

} // namespace haylen::gltf
