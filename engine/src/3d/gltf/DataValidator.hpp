#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "3d/gltf/Diagnostics.hpp"
#include "haylen/3d/gltf/Document.hpp"

namespace haylen::gltf {

// Checks the rules of glTF 2.0 about the data that accessors hold, in one pass over each accessor: finite floats, bounds that match the data, unit normals, tangents and rotations, colors within their range, indices that name a vertex and are never the value that restarts a strip, inverse bind matrices without projection, times that grow from zero, sparse indices that grow and stay below the count, and joints and weights that name joints of the skin, never twice for one vertex, with weights that sum to one. The thresholds of the lengths and the sums are those of the official validator of glTF.
class DataValidator final {
  public:
    static void validate(const Document& document, const Diagnostics& diagnostics);

  private:
    // What one accessor must obey, with the use that asked for each rule, which its message names.
    struct Checks {
        std::string normals;
        std::string tangents;
        std::string colors;
        std::string times;
        std::string rotations;
        bool cubicSpline = false;
        std::string indices;
        std::uint64_t vertices = 0;
        std::string inverseBindMatrices;
        std::uint64_t joints = 0;
    };

    DataValidator(const Document& target, const Diagnostics& documentDiagnostics);

    void collect();
    void validateSparse(std::size_t accessor) const;
    void validateAccessor(std::size_t accessor) const;
    void validateSkinning() const;
    void validateInfluences(const Mesh::Primitive& primitive, std::uint64_t joints, const std::string& owner) const;

    [[noreturn]] void fail(ReadError::Problem problem, std::size_t accessor, std::uint64_t element, const std::string& use, std::string_view rule) const;

    const Document& document;
    const Diagnostics& diagnostics;
    std::vector<Checks> checks;
};

} // namespace haylen::gltf
