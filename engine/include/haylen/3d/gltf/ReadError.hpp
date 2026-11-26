#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace haylen::gltf {

// The failure of reading a glTF file, whose message names the file, the object of the file in words and the rule it breaks, and says what to do. The problem tells the rule apart for code and tests.
class ReadError final : public std::runtime_error {
  public:
    enum class Problem : std::uint8_t {
        // The container and the text of the document.
        NotGltf,
        GlbHeader,
        GlbLength,
        GlbChunk,
        GlbMissingBinary,
        InvalidJson,
        LimitExceeded,

        // The shape of the objects of the document.
        MissingProperty,
        WrongType,
        InvalidValue,
        DuplicateElement,
        EmptyElement,
        UnsupportedVersion,
        UnsupportedExtension,
        UnsupportedImageFormat,
        UndeclaredExtension,
        InvalidIndex,

        // The resources the document names.
        InvalidUri,
        FileOutsidePackage,
        UriScheme,
        FileOutsideDocument,
        MissingFile,
        ResourceTooShort,
        InvalidImage,

        // The links between buffers, buffer views and accessors.
        BufferViewOutOfRange,
        InvalidByteStride,
        AccessorOutOfRange,
        Misaligned,
        AccessorFormat,
        UsageConflict,

        // Meshes, nodes, skins, animations and cameras.
        InvalidAttribute,
        AttributeCount,
        MorphTargets,
        WeightsCount,
        VertexCount,
        NodeCycle,
        NodeParent,
        SceneRoot,
        NodeTransform,
        SkinMesh,
        SkinHierarchy,
        AnimationTarget,
        InvalidCamera,

        // The data that accessors hold.
        IndexOutOfRange,
        PrimitiveRestart,
        NonFinite,
        BoundsMismatch,
        NotUnit,
        InvalidSign,
        NotClamped,
        InvalidWeights,
        InvalidJoint,
        InvalidInverseBindMatrix,
        InvalidTimes,
        InvalidSparseIndices,
    };

    ReadError(Problem value, const std::string& message) : std::runtime_error(message), problem(value) {}

    [[nodiscard]] Problem getProblem() const noexcept {
        return problem;
    }

  private:
    Problem problem;
};

} // namespace haylen::gltf
