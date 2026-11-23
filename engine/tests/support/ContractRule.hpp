#pragma once

#include <optional>

#include "haylen/gpu/ContractError.hpp"

namespace haylen::test {

// Runs a call of a GPU device that breaks its contract and returns the rule that the device names, or nothing when the call runs.
class ContractRule final {
  public:
    template <typename Call> [[nodiscard]] static std::optional<gpu::ContractError::Rule> of(Call&& call) {
        try {
            call();
        } catch (const gpu::ContractError& error) {
            return error.getRule();
        }

        return std::nullopt;
    }
};

} // namespace haylen::test
