#include "content/ContentTool.hpp"

#include <charconv>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

#include "content/ChannelPublisher.hpp"
#include "content/RecordCache.hpp"
#include "content/ReleaseBuilder.hpp"
#include "content/ReleasePackage.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/io/Package.hpp"

namespace haylen::content {

ContentTool::Arguments::Arguments(std::span<const std::string> words) {
    for (std::size_t index = 0; index < words.size(); ++index) {
        const std::string& word = words[index];
        if (!word.starts_with("--")) {
            positionals.push_back(word);
            continue;
        }
        const std::string name = word.substr(2);
        if (index + 1 < words.size() && !words[index + 1].starts_with("--")) {
            if (!options.try_emplace(name, words[++index]).second) {
                throw std::invalid_argument("The option \"--" + name + "\" is given twice.");
            }
            continue;
        }
        flags.push_back(name);
    }
}

std::string ContentTool::Arguments::takePositional(std::string_view name) {
    if (positionals.empty()) {
        throw std::invalid_argument("The command needs the " + std::string(name) + ".");
    }
    std::string value = std::move(positionals.front());
    positionals.erase(positionals.begin());
    return value;
}

std::optional<std::string> ContentTool::Arguments::takeOptional(std::string_view name) {
    const auto found = options.find(name);
    if (found == options.end()) {
        return std::nullopt;
    }
    std::string value = std::move(found->second);
    options.erase(found);
    return value;
}

std::string ContentTool::Arguments::takeOption(std::string_view name) {
    std::optional<std::string> value = takeOptional(name);
    if (!value) {
        throw std::invalid_argument("The command needs the option \"--" + std::string(name) + "\".");
    }
    return std::move(*value);
}

std::optional<std::uint64_t> ContentTool::Arguments::takeOptionalNumber(std::string_view name) {
    const std::optional<std::string> text = takeOptional(name);
    if (!text) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(text->data(), text->data() + text->size(), value);
    if (error != std::errc() || end != text->data() + text->size()) {
        throw std::invalid_argument("The option \"--" + std::string(name) + "\" takes a whole number, not \"" + *text + "\".");
    }
    return value;
}

std::uint64_t ContentTool::Arguments::takeNumber(std::string_view name) {
    const std::optional<std::uint64_t> value = takeOptionalNumber(name);
    if (!value) {
        throw std::invalid_argument("The command needs the option \"--" + std::string(name) + "\".");
    }
    return *value;
}

bool ContentTool::Arguments::takeFlag(std::string_view name) {
    const auto found = std::ranges::find(flags, name);
    if (found == flags.end()) {
        return false;
    }
    flags.erase(found);
    return true;
}

void ContentTool::Arguments::finish() const {
    if (!positionals.empty()) {
        throw std::invalid_argument("The command takes no argument \"" + positionals.front() + "\".");
    }
    if (!options.empty()) {
        throw std::invalid_argument("The command takes no option \"--" + options.begin()->first + "\".");
    }
    if (!flags.empty()) {
        throw std::invalid_argument("The command takes no option \"--" + flags.front() + "\", or it needs a value.");
    }
}

ContentTool::ContentTool(std::ostream& standardOutput, std::ostream& errorOutput) : output(standardOutput), errors(errorOutput) {}

int ContentTool::run(std::span<const std::string> arguments) {
    try {
        if (arguments.empty()) {
            throw std::invalid_argument("Name a command: \"keys\", \"build\", \"verify\", \"inspect\", \"diff\", \"publish\" or \"compact\".");
        }
        const std::string& command = arguments.front();
        Arguments rest(arguments.subspan(1));
        if (command == "keys") {
            runKeys(rest);
        } else if (command == "build") {
            runBuild(rest);
        } else if (command == "verify") {
            runVerify(rest);
        } else if (command == "inspect") {
            runInspect(rest);
        } else if (command == "diff") {
            runDiff(rest);
        } else if (command == "publish") {
            runPublish(rest);
        } else if (command == "compact") {
            runCompact(rest);
        } else {
            throw std::invalid_argument("The command \"" + command + "\" is unknown. The commands are \"keys\", \"build\", \"verify\", \"inspect\", \"diff\", \"publish\" and \"compact\".");
        }
        return 0;
    } catch (const std::exception& error) {
        errors << "Error: " << error.what() << '\n';
        return 1;
    }
}

void ContentTool::runKeys(Arguments& arguments) {
    const std::string action = arguments.takePositional("action, \"create\", \"rotate\" or \"show\"");
    const std::filesystem::path folder = arguments.takePositional("key folder");
    if (action == "create") {
        const std::string identifier = arguments.takeOption("identifier");
        arguments.finish();
        const KeyStore store = KeyStore::create(folder, identifier);
        output << "Created the keys of \"" << identifier << "\" in \"" << folder.generic_string() << "\".\n";
        printKeys(store);
        return;
    }
    if (action == "rotate") {
        arguments.finish();
        KeyStore store = KeyStore::open(folder);
        store.rotate();
        output << "Added the content key \"" << store.getActiveKeyId().toHex() << "\", which encrypts the content of the next builds, while the earlier keys stay for installed content.\n";
        printKeys(store);
        return;
    }
    if (action != "show") {
        throw std::invalid_argument("The keys command takes \"create\", \"rotate\" or \"show\", not \"" + action + "\".");
    }
    arguments.finish();
    printKeys(KeyStore::open(folder));
}

void ContentTool::printKeys(const KeyStore& store) {
    output << "App \"" << store.getIdentifier() << "\".\n";
    for (const Digest& id : store.getContentKeyIds()) {
        output << "Content key \"" << id.toHex() << "\"" << (id == store.getActiveKeyId() ? ", which encrypts new content.\n" : ".\n");
    }
    const VerifyingKey& verifying = store.getSigningKey()->getVerifyingKey();
    output << "Signing key \"" << verifying.getId().toHex() << "\" with the public key \"" << Digest(verifying.getBytes()).toHex() << "\".\n";
}

void ContentTool::runBuild(Arguments& arguments) {
    const std::filesystem::path app = arguments.takePositional("app folder");
    const KeyStore store = KeyStore::open(arguments.takeOption("keys"));
    ReleaseBuilder::Options options{.profile = arguments.takeOption("profile"), .appBuild = arguments.takeNumber("build")};
    const std::filesystem::path folder = arguments.takeOption("output");
    const std::optional<std::string> earlier = arguments.takeOptional("previous");
    const std::optional<std::string> cache = arguments.takeOptional("cache");
    arguments.finish();

    const std::shared_ptr<const io::Package> package = io::Package::openDirectory(app);
    const core::AppConfig config = core::AppConfig::fromPackage(*package);
    if (config.identifier != store.getIdentifier()) {
        throw std::invalid_argument("The app \"" + config.identifier + "\" cannot build with the keys of \"" + store.getIdentifier() + "\".");
    }
    const ReleaseBuilder builder(store.getContentKeys(), store.getActiveKeyId(), store.getSigningKey(), cache ? std::make_shared<const RecordCache>(*cache) : nullptr);
    const std::optional<std::filesystem::path> previous = earlier ? std::optional<std::filesystem::path>(*earlier) : std::nullopt;
    const ReleaseBuilder::Result result = builder.build(*package, folder, previous, options);
    output << "Built the release of \"" << config.identifier << "\" for the profile \"" << options.profile << "\" in \"" << folder.generic_string() << "\".\n";
    printStatistics("app", result.app.statistics);
    printStatistics("content", result.content.statistics);
}

void ContentTool::printStatistics(std::string_view domain, const ContentBuilder::Statistics& statistics) {
    output << std::format("The {} domain stores {} new chunks of {} bytes in {} bytes, {} of them from the build cache, and reuses {} chunks of {} bytes, while {} earlier shards leave it.\n", domain, statistics.newChunks, statistics.newBytes, statistics.storedBytes, statistics.cachedChunks, statistics.reusedChunks, statistics.reusedBytes, statistics.droppedShards);
}

ReleaseInspector ContentTool::makeInspector(const KeyStore& store) {
    return ReleaseInspector(store.getContentKeys(), {store.getSigningKey()->getVerifyingKey()});
}

void ContentTool::runVerify(Arguments& arguments) {
    const std::filesystem::path folder = arguments.takePositional("release folder");
    const KeyStore store = KeyStore::open(arguments.takeOption("keys"));
    arguments.finish();
    const ReleaseInspector::Verification verification = makeInspector(store).verify(folder);
    output << std::format("Verified the release \"{}\": 2 manifests, {} shards and {} files of {} bytes, with every chunk authenticated.\n", folder.generic_string(), verification.shards, verification.files, verification.bytes);
}

void ContentTool::runInspect(Arguments& arguments) {
    const std::filesystem::path folder = arguments.takePositional("release folder");
    const KeyStore store = KeyStore::open(arguments.takeOption("keys"));
    const bool chunks = arguments.takeFlag("chunks");
    arguments.finish();
    const ReleaseInspector::Report report = makeInspector(store).inspect(folder);
    printDomain(report.app, chunks);
    printDomain(report.content, chunks);
}

void ContentTool::printDomain(const ReleaseInspector::Domain& domain, bool chunks) {
    const Manifest::Envelope& envelope = domain.envelope;
    output << std::format("The {} domain has the manifest \"{}\", generation {} of the profile \"{}\" for app builds {} to {}, with {} shards.\n", getDomainName(envelope.domain), domain.manifest.toHex(), envelope.generation, envelope.profile, envelope.minimumAppBuild, envelope.maximumAppBuild, envelope.shards.size());
    if (!envelope.luaAbi.empty()) {
        output << "  Its Lua bytecode has the ABI \"" << envelope.luaAbi << "\".\n";
    }
    for (std::size_t index = 0; index < envelope.shards.size(); ++index) {
        output << std::format("  Shard {} is \"{}\" of {} bytes.\n", index, envelope.shards[index].getFileName(), envelope.shards[index].fileSize);
    }
    for (const ReleaseInspector::File& file : domain.files) {
        std::uint64_t stored = 0;
        std::vector<std::uint32_t> shards;
        for (const ReleaseInspector::Chunk& chunk : file.chunks) {
            stored += chunk.encodedSize;
            if (std::ranges::find(shards, chunk.shard) == shards.end()) {
                shards.push_back(chunk.shard);
            }
        }
        std::string places;
        for (const std::uint32_t shard : shards) {
            places += (places.empty() ? "" : ", ") + std::to_string(shard);
        }
        output << std::format("  File \"{}\" holds {} bytes of {}, {}, in {} chunks stored in {} bytes{}.\n", file.path, file.size, getKindName(file.kind), getDeliveryName(file.delivery), file.chunks.size(), stored, places.empty() ? "" : " in shard " + places);
        for (const ReleaseInspector::Chunk& chunk : chunks ? file.chunks : std::vector<ReleaseInspector::Chunk>{}) {
            output << std::format("    Chunk at {} holds {} bytes with the content ID \"{}\", stored in {} bytes with the stored ID \"{}\" in shard {}.\n", chunk.offset, chunk.plainSize, chunk.contentId.toHex(), chunk.encodedSize, chunk.storedId.toHex(), chunk.shard);
        }
    }
    const std::uint64_t files = ReleaseInspector::getFileBytes(domain);
    const std::uint64_t distinct = ReleaseInspector::getChunkBytes(domain);
    output << std::format("  The {} files hold {} bytes in {} distinct chunks of {} bytes, stored in {} bytes, so deduplication saves {} bytes.\n", domain.files.size(), files, domain.chunks.size(), distinct, ReleaseInspector::getStoredBytes(domain), files - distinct);
}

void ContentTool::runDiff(Arguments& arguments) {
    const std::filesystem::path before = arguments.takePositional("earlier release folder");
    const std::filesystem::path after = arguments.takePositional("later release folder");
    const KeyStore store = KeyStore::open(arguments.takeOption("keys"));
    arguments.finish();

    const ReleaseInspector inspector = makeInspector(store);
    const ReleaseInspector::Report earlier = inspector.inspect(before);
    const ReleaseInspector::Report later = inspector.inspect(after);
    std::uint64_t download = 0;
    for (const auto& [name, domains] : {std::pair{std::string_view("app"), std::pair{&earlier.app, &later.app}}, std::pair{std::string_view("content"), std::pair{&earlier.content, &later.content}}}) {
        const ReleaseInspector::Difference difference = ReleaseInspector::compare(*domains.first, *domains.second);
        output << std::format("The {} domain adds {} chunks of {} bytes, reuses {} chunks of {} bytes and drops {} chunks of {} bytes, with {} new shards and {} added, {} changed and {} removed files.\n", name, difference.newChunks, difference.newBytes, difference.reusedChunks, difference.reusedBytes, difference.removedChunks, difference.removedBytes, difference.newShards.size(), difference.addedFiles.size(), difference.changedFiles.size(), difference.removedFiles.size());
        for (const auto& [label, paths] : {std::pair{"Added", &difference.addedFiles}, std::pair{"Changed", &difference.changedFiles}, std::pair{"Removed", &difference.removedFiles}}) {
            for (const std::string& path : *paths) {
                output << "  " << label << " \"" << path << "\".\n";
            }
        }
        download += difference.downloadBytes + std::filesystem::file_size(after / ReleasePackage::getManifestFile(domains.second->envelope.domain));
    }
    output << std::format("An app that holds the earlier release downloads about {} bytes to reach the later one.\n", download);
}

void ContentTool::runPublish(Arguments& arguments) {
    const std::filesystem::path app = arguments.takePositional("app folder");
    const KeyStore store = KeyStore::open(arguments.takeOption("keys"));
    ChannelPublisher::Options options{.profile = arguments.takeOption("profile"), .appBuild = arguments.takeNumber("build"), .channel = arguments.takeOption("channel")};
    const std::filesystem::path tree = arguments.takeOption("output");
    const std::optional<std::string> cache = arguments.takeOptional("cache");
    arguments.finish();

    const std::shared_ptr<const io::Package> package = io::Package::openDirectory(app);
    if (core::AppConfig::fromPackage(*package).identifier != store.getIdentifier()) {
        throw std::invalid_argument("The app cannot publish with the keys of \"" + store.getIdentifier() + "\".");
    }
    const ChannelPublisher publisher(store.getContentKeys(), store.getActiveKeyId(), store.getSigningKey(), cache ? std::make_shared<const RecordCache>(*cache) : nullptr);
    const ChannelPublisher::Publication publication = publisher.publish(*package, tree, options);
    std::uint64_t bytes = 0;
    for (const ShardReference& pack : publication.newPacks) {
        bytes += pack.fileSize;
    }
    output << std::format("Published generation {} of the channel \"{}\" in \"{}\" with the manifest \"{}\" and {} new packs of {} bytes.\n", publication.generation, options.channel, tree.generic_string(), publication.manifest.toHex(), publication.newPacks.size(), bytes);
    printStatistics("content", publication.statistics);
}

void ContentTool::runCompact(Arguments& arguments) {
    const std::filesystem::path tree = arguments.takePositional("publication tree");
    const KeyStore store = KeyStore::open(arguments.takeOption("keys"));
    const std::uint64_t keep = arguments.takeOptionalNumber("keep").value_or(kDefaultKeep);
    arguments.finish();

    const ChannelPublisher publisher(store.getContentKeys(), store.getActiveKeyId(), store.getSigningKey());
    const ChannelPublisher::Compaction compaction = publisher.compact(tree, keep);
    output << std::format("Removed {} manifests and {} packs of {} bytes that no channel reaches within its last {} generations.\n", compaction.manifests, compaction.packs, compaction.bytes, keep);
}

std::string_view ContentTool::getDeliveryName(Delivery delivery) noexcept {
    switch (delivery) {
    case Delivery::Required:
        return "required";
    case Delivery::Prefetch:
        return "prefetch";
    case Delivery::OnDemand:
        return "onDemand";
    }
    return "required";
}

std::string_view ContentTool::getKindName(Catalog::Kind kind) noexcept {
    return kind == Catalog::Kind::LuaBytecode ? "Lua bytecode" : "data";
}

std::string_view ContentTool::getDomainName(Manifest::Domain domain) noexcept {
    return domain == Manifest::Domain::App ? "app" : "content";
}

} // namespace haylen::content
