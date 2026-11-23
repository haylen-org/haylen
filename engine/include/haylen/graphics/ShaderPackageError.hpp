#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace haylen::graphics {

// A `.shader` package that the engine refuses, which is damaged or was made by another version of the Haylen shader compiler. Its message names the file and the command that compiles it again, and the rule of the package that it breaks is a value for the code that checks it, never part of the message.
class ShaderPackageError final : public std::invalid_argument {
  public:
    enum class Problem : std::uint8_t {
        Structure,
        Version,
        NoPrograms,
        Stage,
        SlotRange,
        BlockSize,
        TextureType,
        SampleType,
        SamplerType,
        BindingRange,
        MissingBinding,
        MissingSource,
        SharedSlot,
        UnknownPair,
        Outputs,
        UnknownLanguage,
        OpenGlBlock,
        Code,
        MaterialBlock,
        UniformType,
        UniformLayout,
    };

    explicit ShaderPackageError(Problem rule, std::string_view file = {});

    [[nodiscard]] Problem getProblem() const noexcept {
        return problem;
    }

  private:
    [[nodiscard]] static std::string describe(std::string_view file);

    Problem problem;
};

} // namespace haylen::graphics
