#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "content/Delivery.hpp"
#include "content/crypto/Digest.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/crypto/VerifyingKey.hpp"
#include "content/format/Catalog.hpp"
#include "content/format/Manifest.hpp"

namespace haylen::content {

// Reads a protected release folder with the keys of its app, where releases are built: the envelope and the catalog of each manifest, the files with their chunks and shards, and what two releases share. It also checks every byte of a release, so a release that verifies is one the app opens.
class ReleaseInspector final {
  public:
    struct Chunk {
        Digest storedId;
        Digest contentId;
        std::uint64_t offset = 0;
        std::uint64_t plainSize = 0;
        std::uint64_t encodedSize = 0;
        std::uint32_t shard = 0;
    };

    struct File {
        std::string path;
        Delivery delivery = Delivery::Required;
        std::uint64_t size = 0;
        std::vector<Chunk> chunks;
    };

    struct Domain {
        Digest manifest;
        Manifest::Envelope envelope;
        std::vector<File> files;

        // Every chunk the domain stores once, by its stored ID.
        std::map<Digest, Chunk> chunks;
    };

    struct Report {
        Domain app;
        Domain content;
    };

    struct Difference {
        std::uint64_t newChunks = 0;
        std::uint64_t newBytes = 0;
        std::uint64_t reusedChunks = 0;
        std::uint64_t reusedBytes = 0;
        std::uint64_t removedChunks = 0;
        std::uint64_t removedBytes = 0;

        // The bytes an app that holds the earlier release downloads: the stored records of the new chunks.
        std::uint64_t downloadBytes = 0;
        std::vector<ShardReference> newShards;
        std::vector<std::string> addedFiles;
        std::vector<std::string> changedFiles;
        std::vector<std::string> removedFiles;
    };

    struct Verification {
        std::uint64_t shards = 0;
        std::uint64_t files = 0;
        std::uint64_t bytes = 0;
    };

    ReleaseInspector(std::shared_ptr<const KeyRing> contentKeys, std::vector<VerifyingKey> trustedKeys);

    [[nodiscard]] Report inspect(const std::filesystem::path& folder) const;

    // Checks that the folder holds the two manifests and the shards they name and nothing else, every shard against the size and digest its manifest signs, and every file by reading it whole, which authenticates and checks every chunk. Throws the error of the first problem.
    [[nodiscard]] Verification verify(const std::filesystem::path& folder) const;

    [[nodiscard]] static Difference compare(const Domain& before, const Domain& after);

    // The bytes the files hold together, and the bytes of their distinct chunks, whose difference deduplication saves.
    [[nodiscard]] static std::uint64_t getFileBytes(const Domain& domain) noexcept;
    [[nodiscard]] static std::uint64_t getChunkBytes(const Domain& domain) noexcept;
    [[nodiscard]] static std::uint64_t getStoredBytes(const Domain& domain) noexcept;

  private:
    [[nodiscard]] Manifest readManifest(const std::filesystem::path& folder, Manifest::Domain domain) const;
    [[nodiscard]] Domain inspectDomain(const std::filesystem::path& folder, Manifest::Domain domain) const;

    std::shared_ptr<const KeyRing> keys;
    std::vector<VerifyingKey> trusted;
};

} // namespace haylen::content
