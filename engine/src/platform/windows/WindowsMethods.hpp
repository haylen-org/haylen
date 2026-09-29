#pragma once

#include "platform/desktop/DesktopMethods.hpp"

namespace haylen::platform {

// Desktop methods answered by Windows.
class WindowsMethods final : public DesktopMethods {
  private:
    [[nodiscard]] core::Json getDeviceInfo() const override;
    [[nodiscard]] std::string getLocale() const override;
    void openUrl(const std::string& url, std::function<void(bool opened)> done) override;

    [[nodiscard]] static std::string getSystemVersion();
    [[nodiscard]] static std::string narrow(const wchar_t* text);
    [[nodiscard]] static std::wstring widen(const std::string& text);
};

} // namespace haylen::platform
