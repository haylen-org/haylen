#include "content/ReleaseInspector.hpp"

#include <monocypher.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <utility>

#include "content/Compatibility.hpp"
#include "content/Error.hpp"
#include "content/ReleasePackage.hpp"
#include "content/format/ChunkRecord.hpp"
#include "content/format/Chunker.hpp"
#include "haylen/io/Package.hpp"

namespace haylen::content {

ReleaseInspector::ReleaseInspector(std::shared_ptr<const KeyRing> contentKeys, std::vector<VerifyingKey> trustedKeys) : keys(std::move(contentKeys)), trusted(std::move(trustedKeys)) {}

Manifest ReleaseInspector::readManifest(const std::filesystem::path& folder, Manifest::Domain domain) const {
    const std::filesystem::path file = folder / ReleasePackage::getManifestFile(domain);
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("The release folder \"" + folder.generic_string() + "\" has no \"" + file.filename().generic_string() + "\".");
    }
    Manifest manifest = Manifest::read({std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()}, trusted);
    if (manifest.getEnvelope().domain != domain) {
        throw Error(Error::Code::CorruptManifest, "The manifest \"" + file.generic_string() + "\" belongs to the other domain.");
    }
    return manifest;
}

ReleaseInspector::Domain ReleaseInspector::inspectDomain(const std::filesystem::path& folder, Manifest::Domain domain) const {
    const Manifest manifest = readManifest(folder, domain);
    const Catalog catalog = manifest.decryptCatalog(*keys);
    Domain result{.manifest = manifest.getId(), .envelope = manifest.getEnvelope()};
    for (std::uint64_t index = 0; index < catalog.getChunkCount(); ++index) {
        const Catalog::Chunk chunk = catalog.getChunk(index);
        result.chunks.emplace(chunk.storedId, Chunk{.storedId = chunk.storedId, .contentId = chunk.contentId, .plainSize = chunk.plainSize, .encodedSize = chunk.encodedSize, .shard = chunk.shard});
    }

    for (std::uint64_t index = 0; index < catalog.getFileCount(); ++index) {
        const Catalog::File entry = catalog.getFile(index);
        File file{.path = std::string(entry.path), .delivery = entry.delivery, .size = entry.size};
        for (std::uint64_t part = entry.firstPart; part < entry.firstPart + entry.partCount; ++part) {
            const Catalog::Part piece = catalog.getPart(part);
            Chunk chunk = result.chunks.at(catalog.getChunk(piece.chunk).storedId);
            chunk.offset = piece.offset;
            file.chunks.push_back(chunk);
        }
        result.files.push_back(std::move(file));
    }
    return result;
}

ReleaseInspector::Report ReleaseInspector::inspect(const std::filesystem::path& folder) const {
    return {.app = inspectDomain(folder, Manifest::Domain::App), .content = inspectDomain(folder, Manifest::Domain::Content)};
}

ReleaseInspector::Verification ReleaseInspector::verify(const std::filesystem::path& folder) const {
    const Manifest app = readManifest(folder, Manifest::Domain::App);
    const Manifest content = readManifest(folder, Manifest::Domain::Content);
    std::set<std::string> expected = {std::string(ReleasePackage::kAppManifestFile), std::string(ReleasePackage::kContentManifestFile)};
    Verification verification;
    for (const Manifest* manifest : {&app, &content}) {
        for (const ShardReference& shard : manifest->getEnvelope().shards) {
            if (!expected.insert(shard.getFileName()).second) {
                continue;
            }
            const std::filesystem::path file = folder / shard.getFileName();
            if (!std::filesystem::is_regular_file(file)) {
                throw Error(Error::Code::MissingShard, "The release folder has no shard \"" + shard.getFileName() + "\", which its manifests name.");
            }
            if (std::filesystem::file_size(file) != shard.fileSize || Digest::ofFile(file) != shard.fileDigest) {
                throw Error(Error::Code::CorruptHeader, "The shard \"" + shard.getFileName() + "\" is not the file its manifest signs.");
            }
            ++verification.shards;
        }
    }
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folder)) {
        if (!expected.contains(entry.path().filename().generic_string())) {
            throw std::runtime_error("The release folder holds \"" + entry.path().filename().generic_string() + "\", which no manifest names, so it does not belong in a release.");
        }
    }

    // Reading every file whole checks every chunk, with the build range and profile the manifests themselves name.
    const Manifest::Envelope& envelope = app.getEnvelope();
    const Compatibility compatibility{.application = envelope.application, .appBuild = envelope.minimumAppBuild, .profile = envelope.profile};
    const std::unique_ptr<io::Package> package = ReleasePackage::open(io::Package::openDirectory(folder), keys, trusted, compatibility);
    std::vector<std::uint8_t> buffer(Chunker::kMaximumSize);
    for (const std::string& path : package->list("")) {
        const std::unique_ptr<io::PackageReader> reader = package->openReader(path);
        for (std::uint64_t offset = 0; offset < reader->getSize();) {
            const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), reader->getSize() - offset));
            reader->readExactly(offset, std::span(buffer).first(count));
            offset += count;
        }
        ++verification.files;
        verification.bytes += reader->getSize();
    }
    crypto_wipe(buffer.data(), buffer.size());
    return verification;
}

ReleaseInspector::Difference ReleaseInspector::compare(const Domain& before, const Domain& after) {
    Difference difference;
    for (const auto& [storedId, chunk] : after.chunks) {
        if (before.chunks.contains(storedId)) {
            ++difference.reusedChunks;
            difference.reusedBytes += chunk.plainSize;
            continue;
        }
        ++difference.newChunks;
        difference.newBytes += chunk.plainSize;
        difference.downloadBytes += ChunkRecord::kHeaderSize + chunk.encodedSize;
    }
    for (const auto& [storedId, chunk] : before.chunks) {
        if (!after.chunks.contains(storedId)) {
            ++difference.removedChunks;
            difference.removedBytes += chunk.plainSize;
        }
    }
    for (const ShardReference& shard : after.envelope.shards) {
        if (std::ranges::find(before.envelope.shards, shard) == before.envelope.shards.end()) {
            difference.newShards.push_back(shard);
        }
    }

    // A file changed when its chunks are not the same chunks in the same order.
    std::map<std::string, std::vector<Digest>> earlier;
    for (const File& file : before.files) {
        std::vector<Digest>& ids = earlier[file.path];
        std::ranges::transform(file.chunks, std::back_inserter(ids), &Chunk::storedId);
    }
    for (const File& file : after.files) {
        std::vector<Digest> ids;
        std::ranges::transform(file.chunks, std::back_inserter(ids), &Chunk::storedId);
        const auto found = earlier.find(file.path);
        if (found == earlier.end()) {
            difference.addedFiles.push_back(file.path);
            continue;
        }
        if (found->second != ids) {
            difference.changedFiles.push_back(file.path);
        }
        earlier.erase(found);
    }
    for (const auto& [path, ids] : earlier) {
        difference.removedFiles.push_back(path);
    }
    return difference;
}

std::uint64_t ReleaseInspector::getFileBytes(const Domain& domain) noexcept {
    std::uint64_t total = 0;
    for (const File& file : domain.files) {
        total += file.size;
    }
    return total;
}

std::uint64_t ReleaseInspector::getChunkBytes(const Domain& domain) noexcept {
    std::uint64_t total = 0;
    for (const auto& [storedId, chunk] : domain.chunks) {
        total += chunk.plainSize;
    }
    return total;
}

std::uint64_t ReleaseInspector::getStoredBytes(const Domain& domain) noexcept {
    std::uint64_t total = 0;
    for (const auto& [storedId, chunk] : domain.chunks) {
        total += ChunkRecord::kHeaderSize + chunk.encodedSize;
    }
    return total;
}

} // namespace haylen::content
