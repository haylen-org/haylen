#pragma once

#include <stdexcept>
#include <string>

namespace haylen::content {

// A failure of protected content, named by a code so callers can tell a damaged install from a missing key or a rejected update. Messages name files, shards and IDs, never keys or decrypted bytes.
class Error final : public std::runtime_error {
  public:
    enum class Code {
        UnsupportedFormat,
        UnsupportedVersion,
        CorruptHeader,
        CorruptIndex,
        CorruptChunk,
        CorruptCatalog,
        CorruptManifest,
        InvalidOffset,
        ChunkAuthenticationFailed,
        ChunkHashMismatch,
        CatalogAuthenticationFailed,
        ManifestSignatureInvalid,
        ManifestIncompatible,
        ManifestRollbackRejected,
        UnknownKeyId,
        MissingChunk,
        MissingShard,
    };

    Error(Code errorCode, const std::string& message);

    [[nodiscard]] Code getCode() const noexcept {
        return code;
    }

  private:
    Code code;
};

} // namespace haylen::content
