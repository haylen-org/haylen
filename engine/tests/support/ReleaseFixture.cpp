#include "support/ReleaseFixture.hpp"

#include <optional>
#include <utility>
#include <vector>

#include "content/ReleasePackage.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "support/TestFiles.hpp"

namespace haylen::test {

ReleaseFixture::ReleaseFixture() : signingKey(std::make_shared<const content::SigningKey>(makeKey(101))) {
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
    return domain == content::Manifest::Domain::App ? result.app.statistics : result.content.statistics;
}

content::ReleaseBuilder ReleaseFixture::makeBuilder() const {
    return {keys, contentKeyId, signingKey};
}

void ReleaseFixture::build(const std::map<std::string, std::string>& files, std::uint64_t shardTarget) {
    std::map<std::string, std::vector<std::uint8_t>> contents;
    for (const auto& [path, text] : files) {
        contents.emplace(path, TestFiles::bytes(text));
    }
    build(io::MemoryPackage("source", std::move(contents)), shardTarget);
}

void ReleaseFixture::build(const io::Package& source, std::uint64_t shardTarget) {
    const std::optional<std::filesystem::path> earlier = builds > 0 ? std::optional(folder) : std::nullopt;
    folder = directory.getPath() / ("release-" + std::to_string(++builds));
    result = makeBuilder().build(source, folder, earlier, {.profile = std::string(kProfile), .appBuild = kAppBuild, .shardTarget = shardTarget});
}

std::shared_ptr<const io::Package> ReleaseFixture::getFiles() const {
    return io::Package::openDirectory(folder);
}

std::unique_ptr<io::Package> ReleaseFixture::open(std::shared_ptr<const io::Package> files) const {
    const content::VerifyingKey trusted = signingKey->getVerifyingKey();
    return content::ReleasePackage::open(std::move(files), keys, std::span(&trusted, 1), getCompatibility());
}

std::unique_ptr<io::Package> ReleaseFixture::open() const {
    return open(getFiles());
}

} // namespace haylen::test
