#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "haylen/3d/gltf/ReadError.hpp"

namespace haylen::gltf {

// Writes the sentences of the failures of a glTF file, which name the file, the object of the file in words, such as primitive 1 of mesh 0, and the rule it breaks, and say what to do.
class Diagnostics final {
  public:
    // What fixes a file that breaks a rule of glTF 2.0.
    static constexpr std::string_view kExportAgain = "Export the model again from the tool that made it, so it follows glTF 2.0.";

    explicit Diagnostics(std::string fileName) : file(std::move(fileName)) {}

    [[nodiscard]] const std::string& getFile() const noexcept {
        return file;
    }

    // Throws for a file that breaks a rule of glTF 2.0, as `The glTF file "<file>" is not valid, since <reason>. <advice>`.
    [[noreturn]] void invalid(ReadError::Problem problem, std::string_view reason, std::string_view advice = kExportAgain) const;

    // Throws for a file that the reader refuses for another reason, as `The glTF file "<file>" <predicate>. <advice>`.
    [[noreturn]] void refuse(ReadError::Problem problem, std::string_view predicate, std::string_view advice) const;

    // Names an object of the file by its kind and its index, such as `mesh 3`, or one inside another, such as `primitive 1 of mesh 3`.
    [[nodiscard]] static std::string item(std::string_view kind, std::size_t index);
    [[nodiscard]] static std::string item(std::string_view kind, std::size_t index, std::string_view owner);

    // Names a key of an object, such as `the key "indices" of primitive 1 of mesh 3`.
    [[nodiscard]] static std::string key(std::string_view name, std::string_view owner);

  private:
    std::string file;
};

} // namespace haylen::gltf
