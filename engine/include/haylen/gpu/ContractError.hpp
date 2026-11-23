#pragma once

#include <cstdint>
#include <stdexcept>

namespace haylen::gpu {

// A call that breaks the contract of `gpu::Device`, such as a draw outside a pass or a buffer that does not exist, which only a bug of the engine or of a port makes. Its message tells the person to report it, and the rule that the call breaks is a value for the code that checks the contract, never part of the message.
class ContractError final : public std::logic_error {
  public:
    enum class Rule : std::uint8_t {
        MissingObject,
        Buffer,
        Image,
        Sampler,
        Program,
        Pipeline,
        PassOrder,
        PassAttachments,
        PipelineState,
        Bindings,
        Uniforms,
        Draw,
        BufferWrite,
        ImageWrite,
        Readback,
        Surface,
        DebugGroup,
        Capture,
        Context,
    };

    explicit ContractError(Rule broken);

    [[nodiscard]] Rule getRule() const noexcept {
        return rule;
    }

  private:
    Rule rule;
};

} // namespace haylen::gpu
