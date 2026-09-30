#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/platform/Battery.hpp"

namespace haylen::platform {

// Reads what Linux tells about the device from the texts of its files and variables: the distribution in `/etc/os-release`, the processor in `/proc/cpuinfo`, the batteries in `/sys/class/power_supply`, the languages in `LANGUAGE` and `LANG` and the time zone in `TZ` and the link `/etc/localtime`. It compiles on every platform, so the tests of every host check it with files of their own.
class LinuxSystemReader final {
  public:
    // The whole text of a file, which is empty when the file cannot be read.
    [[nodiscard]] static std::string readText(const std::filesystem::path& file);

    // The first line of a file without the spaces around it, as the attributes of `/sys` hold their values.
    [[nodiscard]] static std::string readValue(const std::filesystem::path& file);

    // The distribution that `PRETTY_NAME` names, or `VERSION_ID` without it, followed by the release of the kernel, such as `Ubuntu 24.04.1 LTS (Linux 6.8.0-45-generic)`.
    [[nodiscard]] static std::string readOsVersion(std::string_view osRelease, std::string_view kernel);

    // The `model name` of the first processor, which ARM processors often leave out.
    [[nodiscard]] static std::string readCpuName(std::string_view cpuInfo);

    // The first battery of the system among the power supplies in the folder, leaving out the batteries of devices such as mice, and `None` without one.
    [[nodiscard]] static Battery readBattery(const std::filesystem::path& powerSupplies);

    // The locale of a variable such as `LANG` as a BCP 47 tag, such as `pt-BR` for `pt_BR.UTF-8`. The C and POSIX locales name no language, so their tag is empty.
    [[nodiscard]] static std::string toLanguageTag(std::string_view locale);

    // The languages of `LANGUAGE`, a list in the order of preference separated by colons, followed by the one of `LANG`, each once.
    [[nodiscard]] static std::vector<std::string> readLanguages(std::string_view language, std::string_view lang);

    // The IANA name that `TZ` gives, with or without a leading colon or as a file of the time zone database, and without `TZ` the name at the end of the target of the link `/etc/localtime`.
    [[nodiscard]] static std::string readTimeZone(std::string_view tz, std::string_view localTime);

  private:
    [[nodiscard]] static std::vector<std::string_view> splitLines(std::string_view text);
    [[nodiscard]] static std::string_view trim(std::string_view text);
    [[nodiscard]] static std::string unquote(std::string_view value);
    [[nodiscard]] static Battery readSupply(const std::filesystem::path& supply);
    static void addLanguage(std::vector<std::string>& tags, std::string_view locale);
    [[nodiscard]] static std::string getZoneName(std::string_view path);
    [[nodiscard]] static bool isZoneName(std::string_view name);
};

} // namespace haylen::platform
