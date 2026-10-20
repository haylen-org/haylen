#include "content/ReleaseBuilder.hpp"

#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "content/LuaBytecodeAbi.hpp"
#include "content/LuaCompiler.hpp"
#include "content/ReleasePackage.hpp"
#include "content/format/ManifestWriter.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::content {

ReleaseBuilder::ReleaseBuilder(std::shared_ptr<const KeyRing> contentKeys, const Digest& activeKeyId, std::shared_ptr<const SigningKey> signing, std::shared_ptr<const RecordCache> recordCache) : keys(std::move(contentKeys)), activeKey(keys->get(activeKeyId)), signingKey(std::move(signing)), cache(std::move(recordCache)) {}

ReleaseBuilder::Result ReleaseBuilder::build(const io::Package& app, const std::filesystem::path& output, const std::optional<std::filesystem::path>& earlier, const Options& options) const {
    const core::AppConfig config = core::AppConfig::fromPackage(app);
    if (std::filesystem::exists(output)) {
        throw std::invalid_argument("The release folder \"" + output.generic_string() + "\" exists already. Build into a new folder.");
    }
    const EarlierFiles files = earlier ? findEarlierFiles(*earlier) : EarlierFiles{};
    std::filesystem::create_directories(output);
    return {.app = buildRelease(app, config, Manifest::Domain::App, output, files, options), .content = buildRelease(app, config, Manifest::Domain::Content, output, files, options)};
}

ReleaseBuilder::EarlierFiles ReleaseBuilder::findEarlierFiles(const std::filesystem::path& folder) {
    if (!std::filesystem::is_directory(folder)) {
        throw std::invalid_argument("The folder \"" + folder.generic_string() + "\" of the previous release does not exist.");
    }
    EarlierFiles files;
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(folder)) {
        const std::string name = entry.path().filename().string();
        const bool manifest = name == ReleasePackage::kAppManifestFile || name == ReleasePackage::kContentManifestFile;
        if (!entry.is_regular_file() || (!manifest && entry.path().extension() != ShardReference::kExtension)) {
            continue;
        }
        if (!files.emplace(name, entry.path()).second && manifest) {
            throw std::invalid_argument("The folder \"" + folder.generic_string() + "\" holds \"" + name + "\" more than once, so it holds more than one release. Name the folder of one release.");
        }
    }
    for (const std::string_view manifest : {ReleasePackage::kAppManifestFile, ReleasePackage::kContentManifestFile}) {
        if (!files.contains(std::string(manifest))) {
            throw std::invalid_argument("The folder \"" + folder.generic_string() + "\" holds no \"" + std::string(manifest) + "\", so it holds no release to follow.");
        }
    }
    return files;
}

ReleaseBuilder::Domain ReleaseBuilder::buildRelease(const io::Package& app, const core::AppConfig& config, Manifest::Domain domain, const std::filesystem::path& output, const EarlierFiles& earlier, const Options& options) const {
    const std::string_view manifestFile = ReleasePackage::getManifestFile(domain);
    std::optional<Manifest> previous;
    if (!earlier.empty()) {
        previous = readManifest(earlier.at(std::string(manifestFile)));
    }

    // A release that ships with one build of the app serves that build alone, so it names no previous manifest and no channel.
    Manifest::Envelope envelope{.domain = domain, .application = Manifest::identifyApplication(config.identifier), .profile = options.profile, .generation = options.generation, .minimumAppBuild = options.appBuild, .maximumAppBuild = options.appBuild};
    std::vector<ContentBuilder::Input> inputs = listFiles(app, config, domain);
    std::unique_ptr<io::MemoryPackage> compiled;
    if (domain == Manifest::Domain::App) {
        compiled = compileModules(app, inputs);
        envelope.luaAbi = LuaBytecodeAbi::get();
    }
    DomainBuild built = buildDomain(compiled ? *compiled : app, std::move(inputs), std::move(envelope), previous ? &*previous : nullptr, earlier, output, options.shardTarget);
    for (std::size_t shard = 0; shard < built.result.keptShards; ++shard) {
        keepShard(earlier.at(built.result.shards[shard].getFileName()), output, built.result.shards[shard]);
    }

    std::ofstream stream(output / manifestFile, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(built.manifest.data()), static_cast<std::streamsize>(built.manifest.size()));
    stream.close();
    if (!stream) {
        throw std::runtime_error("The manifest \"" + (output / manifestFile).generic_string() + "\" could not be written.");
    }

    const VerifyingKey trusted = signingKey->getVerifyingKey();
    const Digest id = Manifest::read(std::move(built.manifest), std::span(&trusted, 1)).getId();
    return {.manifest = id, .shards = std::move(built.result.shards), .keptShards = built.result.keptShards, .statistics = built.result.statistics};
}

ReleaseBuilder::DomainBuild ReleaseBuilder::buildDomain(const io::Package& source, std::vector<ContentBuilder::Input> inputs, Manifest::Envelope envelope, const Manifest* earlier, const EarlierFiles& files, const std::filesystem::path& folder, std::uint64_t shardTarget) const {
    ContentBuilder builder(folder, activeKey, shardTarget, cache);
    if (earlier != nullptr) {
        const Manifest::Envelope& before = earlier->getEnvelope();
        if (before.domain != envelope.domain || before.application != envelope.application) {
            throw std::invalid_argument("The earlier manifest \"" + earlier->getId().toHex() + "\" belongs to another app or domain, so the build cannot reuse it.");
        }
        std::vector<bool> held;
        for (const ShardReference& shard : before.shards) {
            held.push_back(files.contains(shard.getFileName()));
        }
        builder.reuse(earlier->decryptCatalog(*keys), before.shards, held);
    }

    DomainBuild built{.result = builder.build(source, std::move(inputs))};
    envelope.shards = built.result.shards;
    built.manifest = ManifestWriter::write(std::move(envelope), built.result.catalog, *activeKey, *signingKey);
    return built;
}

Manifest ReleaseBuilder::readManifest(const std::filesystem::path& file) const {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("The manifest \"" + file.generic_string() + "\" could not be read.");
    }
    const VerifyingKey trusted = signingKey->getVerifyingKey();
    return Manifest::read({std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()}, std::span(&trusted, 1));
}

std::unique_ptr<io::MemoryPackage> ReleaseBuilder::compileModules(const io::Package& app, std::vector<ContentBuilder::Input>& inputs) {
    std::map<std::string, std::vector<std::uint8_t>> files;
    for (ContentBuilder::Input& input : inputs) {
        std::vector<std::uint8_t> bytes = app.read(input.path);
        if (Manifest::isLuaModule(input.path)) {
            bytes = LuaCompiler::compile({reinterpret_cast<const char*>(bytes.data()), bytes.size()}, input.path);
            input.kind = Catalog::Kind::LuaBytecode;
        }
        files.emplace(input.path, std::move(bytes));
    }
    return std::make_unique<io::MemoryPackage>("app", std::move(files));
}

std::vector<ContentBuilder::Input> ReleaseBuilder::listFiles(const io::Package& app, const core::AppConfig& config, Manifest::Domain domain) {
    std::vector<std::string> paths;
    if (domain == Manifest::Domain::Content) {
        paths = app.list(io::Path::kContentDirectory);
    } else {
        paths = app.list(io::Path::kSourceDirectory);
        paths.emplace_back(io::Path::kAppConfigFile);
        for (const auto& [id, values] : config.plugins.items()) {
            paths.push_back(io::Path::plugin(id, io::Path::kPluginManifestFile));
            std::vector<std::string> sources = app.list(io::Path::plugin(id, io::Path::kSourceDirectory));
            paths.insert(paths.end(), std::make_move_iterator(sources.begin()), std::make_move_iterator(sources.end()));
        }
    }

    std::vector<ContentBuilder::Input> inputs;
    for (std::string& path : paths) {
        if (path.substr(path.rfind('/') + 1) == kFolderFile) {
            continue;
        }
        if (domain == Manifest::Domain::App) {
            inputs.push_back({.path = std::move(path)});
            continue;
        }
        const std::string_view asset = std::string_view(path).substr(io::Path::kContentDirectory.size() + 1);
        const Delivery delivery = config.content.getDelivery(asset);
        std::string pack(config.content.getPack(asset));
        inputs.push_back({.path = std::move(path), .delivery = delivery, .pack = std::move(pack)});
    }
    return inputs;
}

void ReleaseBuilder::keepShard(const std::filesystem::path& source, const std::filesystem::path& output, const ShardReference& shard) {
    // A kept shard is the file the earlier manifest signs, so a damaged one stops the build instead of reaching the new release.
    std::error_code error;
    if (std::filesystem::file_size(source, error) != shard.fileSize || error || Digest::ofFile(source) != shard.fileDigest) {
        throw std::runtime_error("The earlier release holds a damaged shard \"" + source.generic_string() + "\". Delete that folder, and the next build starts afresh.");
    }

    // Shards never change after they are written, so the new release links the file where the file system can.
    std::filesystem::create_hard_link(source, output / shard.getFileName(), error);
    if (error) {
        std::filesystem::copy_file(source, output / shard.getFileName());
    }
}

} // namespace haylen::content
