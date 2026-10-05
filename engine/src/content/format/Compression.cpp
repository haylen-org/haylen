#include "content/format/Compression.hpp"

#include <zstd.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "content/Error.hpp"

namespace haylen::content {

Compression::Encoded Compression::encode(std::span<const std::uint8_t> plain) {
    // Each thread keeps its contexts, so chunks compress without allocating the tables of the encoder again.
    thread_local const std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> context(ZSTD_createCCtx(), &ZSTD_freeCCtx);

    std::vector<std::uint8_t> compressed(ZSTD_compressBound(plain.size()));
    ZSTD_CCtx_reset(context.get(), ZSTD_reset_session_and_parameters);
    ZSTD_CCtx_setParameter(context.get(), ZSTD_c_compressionLevel, kZstdLevel);
    ZSTD_CCtx_setParameter(context.get(), ZSTD_c_checksumFlag, 0);
    ZSTD_CCtx_setParameter(context.get(), ZSTD_c_contentSizeFlag, 1);
    ZSTD_CCtx_setParameter(context.get(), ZSTD_c_dictIDFlag, 0);
    const std::size_t size = ZSTD_compress2(context.get(), compressed.data(), compressed.size(), plain.data(), plain.size());
    if (ZSTD_isError(size) != 0) {
        throw std::runtime_error("A chunk could not be compressed, and Zstandard reported \"" + std::string(ZSTD_getErrorName(size)) + "\".");
    }

    if (size < plain.size() && size + plain.size() / kMinimumSavingDivisor <= plain.size()) {
        compressed.resize(size);
        return {.codec = Codec::Zstd, .profile = kZstdProfile, .bytes = std::move(compressed)};
    }
    return {.codec = Codec::None, .profile = kNoneProfile, .bytes = std::vector<std::uint8_t>(plain.begin(), plain.end())};
}

bool Compression::isSupported(std::uint8_t codec, std::uint8_t profile) noexcept {
    return (codec == static_cast<std::uint8_t>(Codec::None) && profile == kNoneProfile) || (codec == static_cast<std::uint8_t>(Codec::Zstd) && profile == kZstdProfile);
}

void Compression::decode(Codec codec, std::span<const std::uint8_t> encoded, std::span<std::uint8_t> plain) {
    if (codec == Codec::None) {
        if (encoded.size() != plain.size()) {
            throw Error(Error::Code::CorruptChunk, "A stored chunk does not have the size its record declares.");
        }
        std::ranges::copy(encoded, plain.begin());
        return;
    }

    // The window limit rejects frames that ask for more memory than any chunk needs.
    thread_local const std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> context(ZSTD_createDCtx(), &ZSTD_freeDCtx);
    ZSTD_DCtx_reset(context.get(), ZSTD_reset_session_and_parameters);
    ZSTD_DCtx_setParameter(context.get(), ZSTD_d_windowLogMax, kMaximumWindowLog);
    if (ZSTD_getFrameContentSize(encoded.data(), encoded.size()) != plain.size()) {
        throw Error(Error::Code::CorruptChunk, "A compressed chunk does not declare the size its record declares.");
    }
    const std::size_t size = ZSTD_decompressDCtx(context.get(), plain.data(), plain.size(), encoded.data(), encoded.size());
    if (ZSTD_isError(size) != 0 || size != plain.size()) {
        throw Error(Error::Code::CorruptChunk, "A compressed chunk could not be decoded to the size its record declares.");
    }
}

} // namespace haylen::content
