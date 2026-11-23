#include "haylen/graphics/ShaderPackageError.hpp"

namespace haylen::graphics {

ShaderPackageError::ShaderPackageError(Problem rule, std::string_view file) : std::invalid_argument(describe(file)), problem(rule) {}

std::string ShaderPackageError::describe(std::string_view file) {
    const std::string subject = file.empty() ? std::string("The shader file") : "The shader file \"" + std::string(file) + "\"";
    return subject + " is damaged or was made by another version of the Haylen shader compiler. Compile it again with \"haylen app shaders\".";
}

} // namespace haylen::graphics
