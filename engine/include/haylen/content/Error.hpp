#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace haylen::content {

// A failure of protected content, named by a code so callers can tell a damaged install from a missing key, a network that is down or a rejected update, with what else is known about it. Messages and details name files the app knows, manifests, packs and IDs, and never keys or decrypted bytes.
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
        KeyUnavailable,
        MissingChunk,
        MissingShard,
        MissingManifest,
        LuaBytecodeIncompatible,
        NetworkOffline,
        NetworkTimeout,
        NetworkCancelled,
        NetworkFailed,
        TlsFailed,
        RangeUnsupported,
        UnexpectedHttpStatus,
        UpdateInterrupted,
        DiskFull,
        QuotaExceeded,
        PermissionDenied,
        CacheCorrupt,
        InsufficientStorage,
        StoreAssetUnavailable,
    };

    // What else is known about a failure. Every field is empty when it does not apply. The path is the path of the file in the package that the app reads, the IDs are the hexadecimal IDs of the manifest, the pack, the plain content, the stored record and the content key involved, and the platform is the content profile of the app.
    struct Details {
        std::optional<int> httpStatus;
        std::optional<std::uint64_t> requiredBytes;
        std::optional<std::uint64_t> availableBytes;
        std::string platformMessage;
        std::string path;
        std::string manifestId;
        std::string packId;
        std::string contentId;
        std::string storedId;
        std::string keyId;
        std::string platform;
    };

    Error(Code errorCode, const std::string& message);
    Error(Code errorCode, const std::string& message, Details errorDetails);

    [[nodiscard]] Code getCode() const noexcept {
        return code;
    }
    [[nodiscard]] const Details& getDetails() const noexcept {
        return details;
    }

    // The layers that a failure passes through add what they know, such as the file that a reader reads or the pack of a record.
    [[nodiscard]] Details& getDetails() noexcept {
        return details;
    }

    // Whether trying again may succeed without a new build of the app, such as when a network comes back or a server recovers. A damaged install, a missing key or a rejected manifest stays as it is.
    [[nodiscard]] bool isRetryable() const noexcept;

    // The name of a code as Lua and events read it, such as `networkOffline`.
    [[nodiscard]] static std::string_view getCodeName(Code value) noexcept;

  private:
    static const std::array<std::pair<std::string_view, Code>, 34> kCodeNames;

    Code code;
    Details details;
};

} // namespace haylen::content
