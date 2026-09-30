#include "platform/linux/LinuxSystemReader.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace haylen::platform {

// Files of `/proc` report no size, so the text is read until the stream ends.
std::string LinuxSystemReader::readText(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::string LinuxSystemReader::readValue(const std::filesystem::path& file) {
    const std::string text = readText(file);
    return std::string(trim(std::string_view(text).substr(0, text.find('\n'))));
}

std::string LinuxSystemReader::readOsVersion(std::string_view osRelease, std::string_view kernel) {
    std::string prettyName;
    std::string versionId;
    for (const std::string_view line : splitLines(osRelease)) {
        const std::size_t equals = line.find('=');
        if (line.starts_with('#') || equals == std::string_view::npos) {
            continue;
        }
        const std::string_view key = line.substr(0, equals);
        if (key == "PRETTY_NAME") {
            prettyName = unquote(line.substr(equals + 1));
        } else if (key == "VERSION_ID") {
            versionId = unquote(line.substr(equals + 1));
        }
    }

    const std::string distribution = prettyName.empty() ? versionId : prettyName;
    if (kernel.empty()) {
        return distribution;
    }
    const std::string release = "Linux " + std::string(kernel);
    return distribution.empty() ? release : distribution + " (" + release + ")";
}

std::string LinuxSystemReader::readCpuName(std::string_view cpuInfo) {
    for (const std::string_view line : splitLines(cpuInfo)) {
        const std::size_t colon = line.find(':');
        if (colon != std::string_view::npos && trim(line.substr(0, colon)) == "model name") {
            return std::string(trim(line.substr(colon + 1)));
        }
    }
    return {};
}

// The supplies are links named after their devices, sorted so the first battery is always the same one.
Battery LinuxSystemReader::readBattery(const std::filesystem::path& powerSupplies) {
    std::vector<std::filesystem::path> supplies;
    std::error_code error;
    for (std::filesystem::directory_iterator entry(powerSupplies, error); !error && entry != std::filesystem::directory_iterator(); entry.increment(error)) {
        supplies.push_back(entry->path());
    }
    std::ranges::sort(supplies);

    for (const std::filesystem::path& supply : supplies) {
        if (readValue(supply / "type") == "Battery" && readValue(supply / "scope") != "Device" && readValue(supply / "present") != "0") {
            return readSupply(supply);
        }
    }
    return {.state = Battery::State::None};
}

std::string LinuxSystemReader::toLanguageTag(std::string_view locale) {
    std::string tag(locale.substr(0, locale.find_first_of(".@")));
    if (tag == "C" || tag == "POSIX") {
        return {};
    }
    std::ranges::replace(tag, '_', '-');
    return tag;
}

std::vector<std::string> LinuxSystemReader::readLanguages(std::string_view language, std::string_view lang) {
    std::vector<std::string> tags;
    for (std::size_t start = 0; start <= language.size();) {
        const std::size_t end = std::min(language.find(':', start), language.size());
        addLanguage(tags, language.substr(start, end - start));
        start = end + 1;
    }
    addLanguage(tags, lang);
    return tags;
}

// A `TZ` that describes its rules instead of naming a zone, such as `CET-1CEST,M3.5.0,M10.5.0/3`, has no IANA name.
std::string LinuxSystemReader::readTimeZone(std::string_view tz, std::string_view localTime) {
    if (tz.empty()) {
        return getZoneName(localTime);
    }
    if (tz.starts_with(':')) {
        tz.remove_prefix(1);
    }
    if (tz.find("zoneinfo/") != std::string_view::npos) {
        return getZoneName(tz);
    }
    return isZoneName(tz) ? std::string(tz) : std::string();
}

std::vector<std::string_view> LinuxSystemReader::splitLines(std::string_view text) {
    std::vector<std::string_view> lines;
    for (std::size_t start = 0; start < text.size();) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        lines.push_back(trim(text.substr(start, end - start)));
        start = end + 1;
    }
    return lines;
}

std::string_view LinuxSystemReader::trim(std::string_view text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

// Values may be quoted, and double quotes escape their special characters with a backslash.
std::string LinuxSystemReader::unquote(std::string_view value) {
    if (value.size() < 2 || (value.front() != '"' && value.front() != '\'') || value.back() != value.front()) {
        return std::string(value);
    }
    const char quote = value.front();
    value = value.substr(1, value.size() - 2);
    std::string text;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (quote == '"' && value[index] == '\\' && index + 1 < value.size()) {
            ++index;
        }
        text += value[index];
    }
    return text;
}

// A battery on mains power that holds its charge below full, as charge thresholds do, reports that it does not charge, which counts as full, as on the other desktops.
Battery LinuxSystemReader::readSupply(const std::filesystem::path& supply) {
    Battery battery;
    const std::string capacity = readValue(supply / "capacity");
    int percent = 0;
    const auto [end, parsed] = std::from_chars(capacity.data(), capacity.data() + capacity.size(), percent);
    if (parsed == std::errc() && end == capacity.data() + capacity.size() && percent >= 0 && percent <= 100) {
        battery.level = static_cast<float>(percent) / 100.0F;
    }

    const std::string status = readValue(supply / "status");
    if (status == "Charging") {
        battery.charging = true;
        battery.state = Battery::State::Charging;
    } else if (status == "Discharging") {
        battery.state = Battery::State::Discharging;
    } else if (status == "Full" || status == "Not charging") {
        battery.state = Battery::State::Full;
    }
    return battery;
}

void LinuxSystemReader::addLanguage(std::vector<std::string>& tags, std::string_view locale) {
    std::string tag = toLanguageTag(locale);
    if (!tag.empty() && std::ranges::find(tags, tag) == tags.end()) {
        tags.push_back(std::move(tag));
    }
}

std::string LinuxSystemReader::getZoneName(std::string_view path) {
    const std::size_t found = path.find("zoneinfo/");
    return found == std::string_view::npos ? std::string() : std::string(path.substr(found + 9));
}

bool LinuxSystemReader::isZoneName(std::string_view name) {
    return !name.empty() && !name.starts_with('/') && std::ranges::all_of(name, [](char character) { return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '/' || character == '_' || character == '-' || character == '+'; });
}

} // namespace haylen::platform
