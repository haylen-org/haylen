#include "platform/linux/LinuxMethods.hpp"

#include <spawn.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace haylen::platform {

core::Json LinuxMethods::getDeviceInfo() const {
    return {{"model", "PC"}, {"system", "Linux"}, {"systemVersion", getSystemVersion()}, {"locale", getLocale()}};
}

// Turns LANG into a BCP 47 tag, such as pt-BR for pt_BR.UTF-8. The C and POSIX locales are English.
std::string LinuxMethods::getLocale() const {
    const char* value = std::getenv("LANG");
    std::string language = value != nullptr ? value : "";
    language = language.substr(0, language.find_first_of(".@"));
    if (language.empty() || language == "C" || language == "POSIX") {
        return "en-US";
    }
    std::ranges::replace(language, '_', '-');
    return language;
}

// Starts xdg-open without blocking the frame thread, and a helper thread waits for it to tell whether an application took the url.
void LinuxMethods::openUrl(const std::string& url, std::function<void(bool opened)> done) {
    pid_t child = 0;
    const std::array<char*, 3> arguments{const_cast<char*>("xdg-open"), const_cast<char*>(url.c_str()), nullptr};
    if (posix_spawnp(&child, "xdg-open", nullptr, nullptr, arguments.data(), environ) != 0) {
        done(false);
        return;
    }
    // clang-format off
    std::thread([child, done = std::move(done)] {
        int status = 0;
        const bool exited = waitpid(child, &status, 0) == child;
        done(exited && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }).detach();
    // clang-format on
}

// Reports the kernel release, which is the one version every Linux distribution shares.
std::string LinuxMethods::getSystemVersion() {
    utsname info{};
    if (uname(&info) != 0) {
        throw std::runtime_error("The Linux kernel version could not be read.");
    }
    return info.release;
}

} // namespace haylen::platform
