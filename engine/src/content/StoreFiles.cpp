#include "content/StoreFiles.hpp"

namespace haylen::content {

bool StoreFiles::isResident(const std::string&, std::uint64_t, std::uint64_t) const {
    return true;
}

void StoreFiles::load(std::vector<Range>, std::function<void(bool)> done) {
    done(true);
}

void StoreFiles::pin(const std::vector<Range>&) {}

} // namespace haylen::content
