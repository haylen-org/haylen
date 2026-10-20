#include "haylen/content/Error.hpp"

#include <algorithm>

namespace haylen::content {

// clang-format off
const std::array<std::pair<std::string_view, Error::Code>, 34> Error::kCodeNames{{
    {"unsupportedFormat", Code::UnsupportedFormat},
    {"unsupportedVersion", Code::UnsupportedVersion},
    {"corruptHeader", Code::CorruptHeader},
    {"corruptIndex", Code::CorruptIndex},
    {"corruptChunk", Code::CorruptChunk},
    {"corruptCatalog", Code::CorruptCatalog},
    {"corruptManifest", Code::CorruptManifest},
    {"invalidOffset", Code::InvalidOffset},
    {"chunkAuthenticationFailed", Code::ChunkAuthenticationFailed},
    {"chunkHashMismatch", Code::ChunkHashMismatch},
    {"catalogAuthenticationFailed", Code::CatalogAuthenticationFailed},
    {"manifestSignatureInvalid", Code::ManifestSignatureInvalid},
    {"manifestIncompatible", Code::ManifestIncompatible},
    {"manifestRollbackRejected", Code::ManifestRollbackRejected},
    {"unknownKeyId", Code::UnknownKeyId},
    {"keyUnavailable", Code::KeyUnavailable},
    {"missingChunk", Code::MissingChunk},
    {"missingShard", Code::MissingShard},
    {"missingManifest", Code::MissingManifest},
    {"luaBytecodeIncompatible", Code::LuaBytecodeIncompatible},
    {"networkOffline", Code::NetworkOffline},
    {"networkTimeout", Code::NetworkTimeout},
    {"networkCancelled", Code::NetworkCancelled},
    {"networkFailed", Code::NetworkFailed},
    {"tlsFailed", Code::TlsFailed},
    {"rangeUnsupported", Code::RangeUnsupported},
    {"unexpectedHttpStatus", Code::UnexpectedHttpStatus},
    {"updateInterrupted", Code::UpdateInterrupted},
    {"diskFull", Code::DiskFull},
    {"quotaExceeded", Code::QuotaExceeded},
    {"permissionDenied", Code::PermissionDenied},
    {"cacheCorrupt", Code::CacheCorrupt},
    {"insufficientStorage", Code::InsufficientStorage},
    {"storeAssetUnavailable", Code::StoreAssetUnavailable},
}};
// clang-format on

Error::Error(Code errorCode, const std::string& message) : std::runtime_error(message), code(errorCode) {}

Error::Error(Code errorCode, const std::string& message, Details errorDetails) : std::runtime_error(message), code(errorCode), details(std::move(errorDetails)) {}

bool Error::isRetryable() const noexcept {
    switch (code) {
    case Code::NetworkOffline:
    case Code::NetworkTimeout:
    case Code::NetworkCancelled:
    case Code::NetworkFailed:
    case Code::UpdateInterrupted:
    case Code::DiskFull:
    case Code::QuotaExceeded:
    case Code::CacheCorrupt:
    case Code::InsufficientStorage:
    case Code::StoreAssetUnavailable:
        return true;
    case Code::UnexpectedHttpStatus:
        // A server error, a timeout of the server or a request that came too often may pass, while a missing file or a refused request stays.
        return !details.httpStatus || *details.httpStatus >= 500 || *details.httpStatus == 408 || *details.httpStatus == 429;
    default:
        return false;
    }
}

std::string_view Error::getCodeName(Code value) noexcept {
    return std::ranges::find(kCodeNames, value, &std::pair<std::string_view, Code>::second)->first;
}

} // namespace haylen::content
