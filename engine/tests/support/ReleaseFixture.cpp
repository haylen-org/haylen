#include "support/ReleaseFixture.hpp"

#include <fstream>
#include <utility>

#include "content/ReleasePackage.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/io/Path.hpp"
#include "support/TestFiles.hpp"

namespace haylen::test {

ReleaseFixture::ReleaseFixture() : signingKey(makeKey(101)) {
    contentKeyId = keys->add(makeKey(1));
}

std::array<std::uint8_t, 32> ReleaseFixture::makeKey(std::uint8_t first) noexcept {
    std::array<std::uint8_t, 32> key{};
    for (std::size_t index = 0; index < key.size(); ++index) {
        key[index] = static_cast<std::uint8_t>(first + index);
    }
    return key;
}

std::shared_ptr<const content::ContentKey> ReleaseFixture::getContentKey() const {
    return keys->get(contentKeyId);
}

content::Compatibility ReleaseFixture::getCompatibility() const {
    return {.application = content::Manifest::identifyApplication(kIdentifier), .appBuild = kAppBuild, .profile = std::string(kProfile)};
}

const content::ContentBuilder::Statistics& ReleaseFixture::getStatistics(content::Manifest::Domain domain) const {
    return domains.at(domain).statistics;
}

void ReleaseFixture::build(const std::map<std::string, std::string>& files, std::uint64_t shardTarget) {
    std::map<std::string, std::vector<std::uint8_t>> contents;
    for (const auto& [path, text] : files) {
        contents.emplace(path, TestFiles::bytes(text));
    }
    build(io::MemoryPackage("source", std::move(contents)), shardTarget);
}

void ReleaseFixture::build(const io::Package& source, std::uint64_t shardTarget) {
    ++generation;
    buildDomain(source, content::Manifest::Domain::App, shardTarget);
    buildDomain(source, content::Manifest::Domain::Content, shardTarget);
}

void ReleaseFixture::buildDomain(const io::Package& source, content::Manifest::Domain domain, std::uint64_t shardTarget) {
    const bool app = domain == content::Manifest::Domain::App;
    std::vector<content::ContentBuilder::Input> inputs;
    for (const std::string& path : source.list("")) {
        const bool appFile = path == io::Path::kAppConfigFile || io::Path::isInside(path, io::Path::kSourceDirectory) || io::Path::isInside(path, io::Path::kPluginsDirectory);
        if (appFile == app) {
            inputs.push_back({.path = path});
        }
    }

    Domain& state = domains[domain];
    content::ContentBuilder builder(getFolder(), getContentKey(), shardTarget);
    if (state.catalog) {
        builder.reuse(*state.catalog, state.shards);
    }
    content::ContentBuilder::Result result = builder.build(source, std::move(inputs));

    content::Manifest::Envelope envelope{.domain = domain, .application = content::Manifest::identifyApplication(kIdentifier), .profile = std::string(kProfile), .generation = generation, .previousManifest = state.manifestId, .minimumAppBuild = kAppBuild, .maximumAppBuild = kAppBuild, .shards = result.shards};
    std::vector<std::uint8_t> bytes = content::Manifest::write(envelope, result.catalog, *getContentKey(), signingKey);
    std::ofstream(getFolder() / (app ? content::ReleasePackage::kAppManifestFile : content::ReleasePackage::kContentManifestFile), std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

    const content::VerifyingKey trusted = signingKey.getVerifyingKey();
    state.manifestId = content::Manifest::read(std::move(bytes), std::span(&trusted, 1)).getId();
    state.catalog = std::make_unique<content::Catalog>(content::Catalog::parse(std::move(result.catalog), result.shards.size()));
    state.shards = std::move(result.shards);
    state.statistics = result.statistics;
}

std::shared_ptr<const io::Package> ReleaseFixture::getFiles() const {
    return io::Package::openDirectory(getFolder());
}

std::unique_ptr<io::Package> ReleaseFixture::open(std::shared_ptr<const io::Package> files) const {
    const content::VerifyingKey trusted = signingKey.getVerifyingKey();
    return content::ReleasePackage::open(std::move(files), keys, std::span(&trusted, 1), getCompatibility());
}

std::unique_ptr<io::Package> ReleaseFixture::open() const {
    return open(getFiles());
}

} // namespace haylen::test
