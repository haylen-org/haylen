#include "content/Error.hpp"

namespace haylen::content {

Error::Error(Code errorCode, const std::string& message) : std::runtime_error(message), code(errorCode) {}

} // namespace haylen::content
