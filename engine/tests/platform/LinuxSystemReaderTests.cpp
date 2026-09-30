#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "platform/linux/LinuxSystemReader.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::platform {

TEST(LinuxSystemReaderTest, NamesTheDistributionAndTheKernel) {
    const std::string ubuntu = "# The release of the distribution.\nNAME=\"Ubuntu\"\nVERSION_ID=\"24.04\"\nPRETTY_NAME=\"Ubuntu 24.04.1 LTS\"\nID=ubuntu\n";
    EXPECT_EQ(LinuxSystemReader::readOsVersion(ubuntu, "6.8.0-45-generic"), "Ubuntu 24.04.1 LTS (Linux 6.8.0-45-generic)");
    EXPECT_EQ(LinuxSystemReader::readOsVersion(ubuntu, ""), "Ubuntu 24.04.1 LTS");
    EXPECT_EQ(LinuxSystemReader::readOsVersion("NAME=Alpine\nVERSION_ID=3.20.3\n", "6.6.54"), "3.20.3 (Linux 6.6.54)");
    EXPECT_EQ(LinuxSystemReader::readOsVersion("", "6.6.54"), "Linux 6.6.54");

    // Double quotes escape their special characters, while single quotes keep every character.
    EXPECT_EQ(LinuxSystemReader::readOsVersion(R"(PRETTY_NAME="Tiny \"Island\" \\ OS")", ""), R"(Tiny "Island" \ OS)");
    EXPECT_EQ(LinuxSystemReader::readOsVersion(R"(PRETTY_NAME='Tiny \"Island\"')", ""), R"(Tiny \"Island\")");
}

TEST(LinuxSystemReaderTest, ReadsTheModelOfTheFirstProcessor) {
    const std::string cpuInfo = "processor\t: 0\nvendor_id\t: GenuineIntel\nmodel name\t: Intel(R) Core(TM) i7-8700 CPU @ 3.20GHz\n\nprocessor\t: 1\nmodel name\t: Another processor\n";
    EXPECT_EQ(LinuxSystemReader::readCpuName(cpuInfo), "Intel(R) Core(TM) i7-8700 CPU @ 3.20GHz");
    EXPECT_EQ(LinuxSystemReader::readCpuName("processor\t: 0\nBogoMIPS\t: 108.00\nCPU implementer\t: 0x41\n"), "");
}

TEST(LinuxSystemReaderTest, ReadsTheFirstBatteryOfTheSystem) {
    test::TemporaryDirectory supplies;
    EXPECT_EQ(LinuxSystemReader::readBattery(supplies.getPath() / "missing"), Battery{.state = Battery::State::None});
    supplies.write("AC/type", "Mains\n");
    supplies.write("AC/online", "1\n");
    supplies.write("hidpp_battery_0/type", "Battery\n");
    supplies.write("hidpp_battery_0/scope", "Device\n");
    supplies.write("hidpp_battery_0/capacity", "10\n");
    EXPECT_EQ(LinuxSystemReader::readBattery(supplies.getPath()), Battery{.state = Battery::State::None});

    // A computer with two bays reports the battery of the first one, and a bay without a battery is skipped.
    supplies.write("BAT0/type", "Battery\n");
    supplies.write("BAT0/present", "0\n");
    supplies.write("BAT1/type", "Battery\n");
    supplies.write("BAT1/capacity", "76\n");
    supplies.write("BAT1/status", "Discharging\n");
    supplies.write("BAT2/type", "Battery\n");
    supplies.write("BAT2/status", "Charging\n");
    EXPECT_EQ(LinuxSystemReader::readBattery(supplies.getPath()), (Battery{.level = 0.76F, .state = Battery::State::Discharging}));

    const std::vector<std::pair<std::string, Battery>> statuses{
        {"Charging", {.level = 0.76F, .charging = true, .state = Battery::State::Charging}},
        {"Full", {.level = 0.76F, .state = Battery::State::Full}},
        {"Not charging", {.level = 0.76F, .state = Battery::State::Full}},
        {"Unknown", {.level = 0.76F, .state = Battery::State::Unknown}},
    };
    for (const auto& [status, battery] : statuses) {
        supplies.write("BAT1/status", status + "\n");
        EXPECT_EQ(LinuxSystemReader::readBattery(supplies.getPath()), battery) << status;
    }

    // A capacity that is not a percentage leaves the level unknown.
    supplies.write("BAT1/capacity", "unknown\n");
    EXPECT_FALSE(LinuxSystemReader::readBattery(supplies.getPath()).level);
    supplies.write("BAT1/capacity", "140\n");
    EXPECT_FALSE(LinuxSystemReader::readBattery(supplies.getPath()).level);
}

TEST(LinuxSystemReaderTest, TurnsLocalesIntoLanguageTags) {
    EXPECT_EQ(LinuxSystemReader::toLanguageTag("pt_BR.UTF-8"), "pt-BR");
    EXPECT_EQ(LinuxSystemReader::toLanguageTag("sr_RS@latin"), "sr-RS");
    EXPECT_EQ(LinuxSystemReader::toLanguageTag("de"), "de");
    EXPECT_EQ(LinuxSystemReader::toLanguageTag("C.UTF-8"), "");
    EXPECT_EQ(LinuxSystemReader::toLanguageTag("POSIX"), "");
    EXPECT_EQ(LinuxSystemReader::toLanguageTag(""), "");

    EXPECT_EQ(LinuxSystemReader::readLanguages("pt_BR:pt:en", "pt_BR.UTF-8"), (std::vector<std::string>{"pt-BR", "pt", "en"}));
    EXPECT_EQ(LinuxSystemReader::readLanguages("", "en_GB.UTF-8"), (std::vector<std::string>{"en-GB"}));
    EXPECT_EQ(LinuxSystemReader::readLanguages("fr::C", "C"), (std::vector<std::string>{"fr"}));
    EXPECT_TRUE(LinuxSystemReader::readLanguages("", "").empty());
}

TEST(LinuxSystemReaderTest, NamesTheTimeZoneFromTzOrTheLocalTimeLink) {
    EXPECT_EQ(LinuxSystemReader::readTimeZone("", "/usr/share/zoneinfo/America/Sao_Paulo"), "America/Sao_Paulo");
    EXPECT_EQ(LinuxSystemReader::readTimeZone("", "../usr/share/zoneinfo/Etc/UTC"), "Etc/UTC");
    EXPECT_EQ(LinuxSystemReader::readTimeZone("", ""), "");
    EXPECT_EQ(LinuxSystemReader::readTimeZone("Europe/Berlin", "/usr/share/zoneinfo/Etc/UTC"), "Europe/Berlin");
    EXPECT_EQ(LinuxSystemReader::readTimeZone(":Asia/Tokyo", ""), "Asia/Tokyo");
    EXPECT_EQ(LinuxSystemReader::readTimeZone(":/usr/share/zoneinfo/Etc/GMT+3", ""), "Etc/GMT+3");
    EXPECT_EQ(LinuxSystemReader::readTimeZone("CET-1CEST,M3.5.0,M10.5.0/3", "/usr/share/zoneinfo/Etc/UTC"), "");
    EXPECT_EQ(LinuxSystemReader::readTimeZone("/etc/custom-zone", ""), "");
}

TEST(LinuxSystemReaderTest, ReadsWholeTextsAndSingleValues) {
    test::TemporaryDirectory folder;
    folder.write("product_name", "  ThinkPad X1 Carbon Gen 11 \nsecond line\n");
    EXPECT_EQ(LinuxSystemReader::readValue(folder.getPath() / "product_name"), "ThinkPad X1 Carbon Gen 11");
    EXPECT_EQ(LinuxSystemReader::readText(folder.getPath() / "product_name"), "  ThinkPad X1 Carbon Gen 11 \nsecond line\n");
    EXPECT_EQ(LinuxSystemReader::readText(folder.getPath() / "missing"), "");
    EXPECT_EQ(LinuxSystemReader::readValue(folder.getPath() / "missing"), "");
}

} // namespace haylen::platform
