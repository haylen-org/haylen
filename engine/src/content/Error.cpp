#include "haylen/content/Error.hpp"

namespace haylen::content {

Error::Error(Code errorCode, const std::string& message) : std::runtime_error(message), code(errorCode) {}

Error::Error(Code errorCode, const std::string& message, Details errorDetails) : std::runtime_error(message), code(errorCode), details(std::move(errorDetails)) {}

bool Error::isRetryable() const noexcept {
    switch (code) {
    case Code::NetworkOffline:
    case Code::NetworkTimeout:
    case Code::NetworkCancelled:
    case Code::NetworkFailed:
    case Code::UpdateInterrupted:
        return true;
    case Code::UnexpectedHttpStatus:
        // A server error, a timeout of the server or a request that came too often may pass, while a missing file or a refused request stays.
        return !details.httpStatus || *details.httpStatus >= 500 || *details.httpStatus == 408 || *details.httpStatus == 429;
    default:
        return false;
    }
}

} // namespace haylen::content
