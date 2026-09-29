#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "haylen/core/Json.hpp"

namespace haylen::platform {

// Native methods of the desktop platforms: device.info, system.locale, system.openUrl and haptics.vibrate. Each desktop platform supplies the answers.
class DesktopMethods {
  public:
    virtual ~DesktopMethods() = default;

    // Answers a platform call through the bridge relay. Failures carry an object with a message, and a method without a handler fails with the code noHandler.
    void dispatch(std::uint64_t call, std::string_view method, std::string_view paramsJson);

  protected:
    // Device info and the locale answer at once or throw with a message, and opening a url reports whether an application took it, possibly later and from another thread.
    [[nodiscard]] virtual core::Json getDeviceInfo() const = 0;
    [[nodiscard]] virtual std::string getLocale() const = 0;
    virtual void openUrl(const std::string& url, std::function<void(bool opened)> done) = 0;

  private:
    static void fail(std::uint64_t call, const std::string& message);
};

} // namespace haylen::platform
