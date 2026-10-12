#include "content/ReleaseStager.hpp"

#include <set>
#include <stdexcept>
#include <system_error>

#include "content/ReleasePackage.hpp"

namespace haylen::content {

ReleaseStager::ReleaseStager(const ReleaseInspector& releaseInspector) : inspector(releaseInspector) {}

ReleaseStager::Layout ReleaseStager::plan(const std::filesystem::path& release) const {
    const ReleaseInspector::Report report = inspector.inspect(release);
    std::set<std::uint32_t> required;
    for (const ReleaseInspector::File& file : report.content.files) {
        if (file.delivery == Delivery::Required) {
            for (const ReleaseInspector::Chunk& chunk : file.chunks) {
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
        (required.contains(index) ? layout.shipped : layout.deferred).push_back(shards[index].getFileName());
    }
    return layout;
}

ReleaseStager::Layout ReleaseStager::stage(const std::filesystem::path& release, const std::filesystem::path& shipped, const std::optional<std::filesystem::path>& deferred) const {
    if (std::filesystem::exists(shipped)) {
        throw std::invalid_argument("The folder \"" + shipped.generic_string() + "\" exists already. Stage the release into a new folder.");
    }
    const Layout layout = plan(release);
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
