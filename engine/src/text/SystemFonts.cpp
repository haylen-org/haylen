#include "haylen/text/SystemFonts.hpp"

#include <utility>

namespace haylen::text {

bool SystemFonts::request(char32_t codePoint) {
    if (asked.insert(codePoint).second) {
        pending.insert(codePoint);
        requests.push_back(codePoint);
    }
    return pending.contains(codePoint);
}

void SystemFonts::takeRequests(std::vector<char32_t>& codePoints) {
    codePoints.clear();
    codePoints.swap(requests);
}

void SystemFonts::finish(std::span<const char32_t> codePoints) {
    for (const char32_t codePoint : codePoints) {
        pending.erase(codePoint);
    }
}

void SystemFonts::add(Face face) {
    faces.push_back(std::move(face));
    ++revision;
}

void SystemFonts::refuse(std::string name) {
    refused.push_back(std::move(name));
}

std::vector<std::string> SystemFonts::getKnownNames() const {
    std::vector<std::string> names = refused;
    for (const Face& face : faces) {
        names.push_back(face.name);
    }
    return names;
}

} // namespace haylen::text
