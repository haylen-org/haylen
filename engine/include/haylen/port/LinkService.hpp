#pragma once

#include <string_view>

namespace haylen::port {

// The links of a platform that starts a new process of the app for every link, as Windows and Linux do, whose new process hands its link to the instance of the app that runs. A port registers it only for such a platform, while a platform that hands links to the running app itself reports them through `SystemReports::receiveLink`.
class LinkService {
  public:
    LinkService() = default;
    LinkService(const LinkService&) = delete;
    LinkService& operator=(const LinkService&) = delete;
    virtual ~LinkService() = default;

    // Hands a link to the instance of the app with the identifier that runs already, and returns whether it took the link, so the new process ends.
    [[nodiscard]] virtual bool forwardLink(std::string_view identifier, std::string_view url) = 0;

    // Takes the links that later processes of the app forward, for as long as the process runs, and reports each one through `SystemReports::receiveLink`.
    virtual void listenForLinks(std::string_view identifier) = 0;
};

} // namespace haylen::port
