#include "content/format/Compressor.hpp"

#include <zstd.h>

#include <format>
#include <memory>
#include <stdexcept>
#include <utility>

namespace haylen::content {

Compressor::Encoded Compressor::encode(std::span<const std::uint8_t> plain) {
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
        return {.codec = Compression::Codec::Zstd, .profile = Compression::kZstdProfile, .bytes = std::move(compressed)};
    }
    return {.codec = Compression::Codec::None, .profile = Compression::kNoneProfile, .bytes = std::vector<std::uint8_t>(plain.begin(), plain.end())};
}

std::string Compressor::getName() {
    return std::format("zstd-{}-profile-{}", ZSTD_versionNumber(), Compression::kZstdProfile);
}

} // namespace haylen::content
