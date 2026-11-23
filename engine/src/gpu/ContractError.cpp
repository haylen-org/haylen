#include "haylen/gpu/ContractError.hpp"

namespace haylen::gpu {

ContractError::ContractError(Rule broken) : std::logic_error("Haylen hit an internal error in its graphics device. Report it with the steps that led to it."), rule(broken) {}

} // namespace haylen::gpu
