#include "content/ChannelPublisher.hpp"

#include <monocypher.h>

#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

#include "content/HpakPackage.hpp"
#include "content/PublicationLayout.hpp"
#include "content/ReleasePackage.hpp"
#include "content/ShardSet.hpp"
#include "content/format/ChannelWriter.hpp"
#include "content/format/ChunkRecord.hpp"
#include "haylen/content/Error.hpp"
#include "haylen/core/AppConfig.hpp"

namespace haylen::content {

ChannelPublisher::ChannelPublisher(std::shared_ptr<const KeyRing> contentKeys, const Digest& activeKeyId, std::shared_ptr<const SigningKey> signing, std::shared_ptr<const RecordCache> recordCache) : keys(contentKeys), signingKey(signing), builder(std::move(contentKeys), activeKeyId, std::move(signing), std::move(recordCache)) {}

ChannelPublisher::Publication ChannelPublisher::publish(const io::Package& app, const std::filesystem::path& tree, const Options& options) const {
    if (options.channel.empty() || !Manifest::isValidName(options.channel)) {
        throw std::invalid_argument("A publication needs the name of a channel, with at most 64 lowercase letters, digits, dashes and dots, not \"" + options.channel + "\".");
    }
    const core::AppConfig config = core::AppConfig::fromPackage(app);
    const Digest application = Manifest::identifyApplication(config.identifier);
    const std::optional<ChannelDescriptor> current = readChannel(tree, options.channel);
    std::optional<Manifest> earlier;
    if (current) {
        if (current->application != application) {
            throw std::invalid_argument("The channel \"" + options.channel + "\" of the tree belongs to another app.");
        }
        earlier = readManifest(tree, current->manifest);
    } else if (!options.release.empty()) {
        earlier = builder.readManifest(options.release / ReleasePackage::kContentManifestFile);
    }

    // The earlier shards are the packs of the tree once the channel has a generation, which must all be there, and before that the shards of the release that ships with the app that its folder holds.
    ReleaseBuilder::EarlierFiles files;
    for (const ShardReference& shard : earlier ? earlier->getEnvelope().shards : std::vector<ShardReference>{}) {
        const std::filesystem::path path = current ? tree / PublicationLayout::getPackPath(shard) : options.release / shard.getFileName();
        if (current || std::filesystem::exists(path)) {
            files.emplace(shard.getFileName(), path);
        }
    }

    // New packs grow in a staging folder of the tree, so moving them into place never copies them, and a failed publish leaves them where the next one clears them.
    const std::filesystem::path staging = tree / kStagingFolder;
    std::filesystem::remove_all(staging);
    std::filesystem::create_directories(staging);
    Manifest::Envelope envelope{.domain = Manifest::Domain::Content, .application = application, .profile = options.profile, .channel = options.channel, .generation = current ? current->generation + 1 : 1, .previousManifest = current ? current->manifest : Digest(), .minimumAppBuild = options.appBuild, .maximumAppBuild = std::numeric_limits<std::uint64_t>::max()};
    ReleaseBuilder::DomainBuild built = builder.buildDomain(app, ReleaseBuilder::listFiles(app, config, Manifest::Domain::Content), envelope, earlier ? &*earlier : nullptr, files, staging, options.shardTarget);

    Publication publication{.generation = envelope.generation, .statistics = built.result.statistics};

    // The first generation publishes the shards it keeps from the release too, since no generation before it did.
    for (std::size_t index = 0; index < built.result.keptShards && !current; ++index) {
        const ShardReference& pack = built.result.shards[index];
        if (!std::filesystem::exists(tree / PublicationLayout::getPackPath(pack))) {
            std::filesystem::create_directories((tree / PublicationLayout::getPackPath(pack)).parent_path());
            std::filesystem::copy_file(files.at(pack.getFileName()), tree / PublicationLayout::getPackPath(pack));
            publication.newPacks.push_back(pack);
        }
    }
    for (std::size_t index = built.result.keptShards; index < built.result.shards.size(); ++index) {
        const ShardReference& pack = built.result.shards[index];
        placeImmutable(staging / pack.getFileName(), tree / PublicationLayout::getPackPath(pack));
        publication.newPacks.push_back(pack);
    }

    const VerifyingKey trusted = signingKey->getVerifyingKey();
    const Manifest manifest = Manifest::read(built.manifest, std::span(&trusted, 1));
    publication.manifest = manifest.getId();
    writeFile(staging / "manifest", built.manifest);
    placeImmutable(staging / "manifest", tree / PublicationLayout::getManifestPath(manifest.getId()));
    verify(tree, manifest);

    // The channel moves to the new generation only now that every file it reaches is in place and verified.
    const ChannelDescriptor descriptor{.application = application, .channel = options.channel, .generation = envelope.generation, .manifest = manifest.getId()};
    writeFile(staging / "channel", ChannelWriter::write(descriptor, *signingKey));
    std::filesystem::create_directories((tree / PublicationLayout::getChannelPath(options.channel)).parent_path());
    std::filesystem::rename(staging / "channel", tree / PublicationLayout::getChannelPath(options.channel));
    std::filesystem::remove_all(staging);
    return publication;
}

std::optional<ChannelDescriptor> ChannelPublisher::readChannel(const std::filesystem::path& tree, std::string_view channel) const {
    std::ifstream stream(tree / PublicationLayout::getChannelPath(channel), std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    const VerifyingKey trusted = signingKey->getVerifyingKey();
    ChannelDescriptor descriptor = ChannelDescriptor::read(bytes, std::span(&trusted, 1));
    if (descriptor.channel != channel) {
        throw Error(Error::Code::CorruptManifest, "The pointer of the channel \"" + std::string(channel) + "\" names the channel \"" + descriptor.channel + "\".");
    }
    return descriptor;
}

Manifest ChannelPublisher::readManifest(const std::filesystem::path& tree, const Digest& manifest) const {
    std::ifstream stream(tree / PublicationLayout::getManifestPath(manifest), std::ios::binary);
    if (!stream) {
        throw Error(Error::Code::MissingManifest, "The tree has no manifest \"" + manifest.toHex() + "\", which a channel names.");
    }
    const VerifyingKey trusted = signingKey->getVerifyingKey();
    Manifest read = Manifest::read({std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()}, std::span(&trusted, 1));
    if (read.getId() != manifest) {
        throw Error(Error::Code::CorruptManifest, "The file of the manifest \"" + manifest.toHex() + "\" holds another manifest.");
    }
    return read;
}

void ChannelPublisher::verify(const std::filesystem::path& tree, const Manifest& manifest) const {
    for (const ShardReference& pack : manifest.getEnvelope().shards) {
        const std::filesystem::path file = tree / PublicationLayout::getPackPath(pack);
        if (!std::filesystem::is_regular_file(file)) {
            throw Error(Error::Code::MissingShard, "The tree has no pack \"" + pack.getFileName() + "\", which its manifest names.");
        }
        if (std::filesystem::file_size(file) != pack.fileSize || Digest::ofFile(file) != pack.fileDigest) {
            throw Error(Error::Code::CorruptHeader, "The pack \"" + pack.getFileName() + "\" of the tree is not the file its manifest signs.");
        }
    }

    const std::shared_ptr<const io::Package> files = io::Package::openDirectory(tree);
    // clang-format off
    const HpakPackage package("publication", std::make_shared<const Catalog>(manifest.decryptCatalog(*keys)), std::make_shared<const ShardSet>(manifest.getEnvelope().shards, keys, [files](const ShardReference& pack) {
        return files->openReader(PublicationLayout::getPackPath(pack));
    }));
    // clang-format on
    std::vector<std::uint8_t> buffer(ChunkRecord::kMaximumPlainSize);
    for (const std::string& path : package.list("")) {
        const std::unique_ptr<io::PackageReader> reader = package.openReader(path);
        for (std::uint64_t offset = 0; offset < reader->getSize(); offset += buffer.size()) {
            reader->readExactly(offset, std::span(buffer).first(static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), reader->getSize() - offset))));
        }
    }
    crypto_wipe(buffer.data(), buffer.size());
}

void ChannelPublisher::placeImmutable(const std::filesystem::path& source, const std::filesystem::path& destination) {
    if (std::filesystem::exists(destination)) {
        if (std::filesystem::file_size(destination) != std::filesystem::file_size(source) || Digest::ofFile(destination) != Digest::ofFile(source)) {
            throw std::runtime_error("The tree already holds \"" + destination.generic_string() + "\" with other bytes, and a published file never changes. Restore the file, or publish into a new tree.");
        }
        std::filesystem::remove(source);
        return;
    }
    std::filesystem::create_directories(destination.parent_path());
    std::filesystem::rename(source, destination);
}

void ChannelPublisher::writeFile(const std::filesystem::path& file, std::span<const std::uint8_t> bytes) {
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream) {
        throw std::runtime_error("The file \"" + file.generic_string() + "\" could not be written.");
    }
}

ChannelPublisher::Compaction ChannelPublisher::compact(const std::filesystem::path& tree, std::uint64_t keep) const {
    if (keep == 0) {
        throw std::invalid_argument("A compaction keeps at least the current generation of every channel.");
    }

    if (!std::filesystem::is_directory(tree / PublicationLayout::kChannelsFolder)) {
        throw std::invalid_argument("The folder \"" + tree.generic_string() + "\" is not a publication tree, because it has no \"" + std::string(PublicationLayout::kChannelsFolder) + "\" folder.");
    }

    // Every channel keeps its current manifest and the ones before it, up to the count, with every pack they name.
    std::set<std::filesystem::path> kept;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(tree / PublicationLayout::kChannelsFolder)) {
        const std::optional<ChannelDescriptor> descriptor = entry.path().extension() == ".hchannel" ? readChannel(tree, entry.path().stem().generic_string()) : std::nullopt;
        if (!descriptor) {
            continue;
        }
        Digest next = descriptor->manifest;
        for (std::uint64_t generation = 0; generation < keep && !next.isZero() && std::filesystem::exists(tree / PublicationLayout::getManifestPath(next)); ++generation) {
            const Manifest manifest = readManifest(tree, next);
            kept.insert(PublicationLayout::getManifestPath(next));
            for (const ShardReference& pack : manifest.getEnvelope().shards) {
                kept.insert(PublicationLayout::getPackPath(pack));
            }
            next = manifest.getEnvelope().previousManifest;
        }
    }

    Compaction compaction;
    for (const std::string_view folder : {PublicationLayout::kManifestsFolder, PublicationLayout::kPacksFolder}) {
        if (!std::filesystem::is_directory(tree / folder)) {
            continue;
        }
        std::vector<std::filesystem::path> removed;
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(tree / folder)) {
            if (entry.is_regular_file() && !kept.contains(entry.path().lexically_relative(tree))) {
                removed.push_back(entry.path());
            }
        }
        for (const std::filesystem::path& file : removed) {
            compaction.bytes += std::filesystem::file_size(file);
            ++(folder == PublicationLayout::kManifestsFolder ? compaction.manifests : compaction.packs);
            std::filesystem::remove(file);
        }
    }
    return compaction;
}

} // namespace haylen::content
