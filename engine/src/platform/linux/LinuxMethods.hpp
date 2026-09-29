#pragma once

#include "platform/desktop/DesktopMethods.hpp"

namespace haylen::platform {

// Desktop methods answered by Linux.
class LinuxMethods final : public DesktopMethods {
  private:
    [[nodiscard]] core::Json getDeviceInfo() const override;
    [[nodiscard]] std::string getLocale() const override;
    void openUrl(const std::string& url, std::function<void(bool opened)> done) override;

    [[nodiscard]] static std::string getSystemVersion();
};

} // namespace haylen::platform
