#include "content/ReleaseStager.hpp"

#include <set>
#include <stdexcept>
#include <system_error>

#include "content/ReleasePackage.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::content {

ReleaseStager::ReleaseStager(const ReleaseInspector& releaseInspector, core::AppConfig::Content rules) : inspector(releaseInspector), settings(std::move(rules)) {}

ReleaseStager::Layout ReleaseStager::plan(const std::filesystem::path& release, Content content) const {
    // A shard ships when a required file outside every pack uses it, belongs to a pack when only the files of that pack use it, and stays for the server otherwise.
    const ReleaseInspector::Report report = inspector.inspect(release);
    std::set<std::uint32_t> required;
    std::map<std::uint32_t, std::set<std::string>> owners;
    for (const ReleaseInspector::File& file : report.content.files) {
        const std::string pack(settings.getPack(std::string_view(file.path).substr(io::Path::kContentDirectory.size() + 1)));
        for (const ReleaseInspector::Chunk& chunk : file.chunks) {
            owners[chunk.shard].insert(pack);
            if (content == Content::Required && file.delivery == Delivery::Required && pack.empty()) {
                required.insert(chunk.shard);
            }
        }
    }

    Layout layout{.shipped = {std::string(ReleasePackage::kAppManifestFile), std::string(ReleasePackage::kContentManifestFile)}};
    for (const ShardReference& shard : report.app.envelope.shards) {
        layout.shipped.push_back(shard.getFileName());
    }
    const std::vector<ShardReference>& shards = report.content.envelope.shards;
    for (std::uint32_t index = 0; index < shards.size(); ++index) {
        const std::set<std::string>& packs = owners[index];
        if (required.contains(index)) {
            layout.shipped.push_back(shards[index].getFileName());
        } else if (packs.size() == 1 && !packs.begin()->empty()) {
            layout.packs[*packs.begin()].push_back(shards[index].getFileName());
        } else if (content == Content::All) {
            layout.shipped.push_back(shards[index].getFileName());
        } else {
            layout.deferred.push_back(shards[index].getFileName());
        }
    }
    return layout;
}

ReleaseStager::Layout ReleaseStager::stage(const std::filesystem::path& release, const std::filesystem::path& shipped, const std::optional<std::filesystem::path>& deferred, const std::optional<std::filesystem::path>& packs, Content content) const {
    if (std::filesystem::exists(shipped)) {
        throw std::invalid_argument("The folder \"" + shipped.generic_string() + "\" exists already. Stage the release into a new folder.");
    }
    const Layout layout = plan(release, content);
    std::filesystem::create_directories(shipped);
    for (const std::string& name : layout.shipped) {
        place(release / name, shipped / name);
    }
    if (deferred) {
        std::filesystem::create_directories(*deferred);
        for (const std::string& name : layout.deferred) {
            place(release / name, *deferred / name);
        }
    }
    for (const auto& [pack, names] : packs ? layout.packs : std::map<std::string, std::vector<std::string>>{}) {
        std::filesystem::create_directories(*packs / pack);
        for (const std::string& name : names) {
            place(release / name, *packs / pack / name);
        }
    }
    return layout;
}

void ReleaseStager::place(const std::filesystem::path& source, const std::filesystem::path& destination) {
    std::error_code error;
    std::filesystem::create_hard_link(source, destination, error);
    if (error) {
        std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
    }
}

} // namespace haylen::content
