#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "platform/android/AndroidDeviceInfo.hpp"

namespace haylen::platform {

TEST(AndroidDeviceInfoTest, ReadsWhatTheJavaSideTells) {
    const SystemInfo info = AndroidDeviceInfo::toSystemInfo(R"({"osVersion": "16", "deviceModel": "Pixel 9", "manufacturer": "Google", "cpuName": "Tensor G4", "cpuCores": 8, "memoryBytes": 12884901888, "television": false, "tablet": false, "locale": "pt-BR", "languages": ["pt-BR", "en-US"], "timeZone": "America/Sao_Paulo"})");
    EXPECT_EQ(info.os, SystemInfo::Os::Android);
    EXPECT_EQ(info.deviceKind, SystemInfo::DeviceKind::Phone);
    EXPECT_EQ(info.osVersion, "16");
    EXPECT_EQ(info.deviceModel, "Pixel 9");
    EXPECT_EQ(info.manufacturer, "Google");
    EXPECT_EQ(info.cpuName, "Tensor G4");
    EXPECT_EQ(info.cpuCores, 8);
    EXPECT_EQ(info.memoryBytes, 12884901888U);
    EXPECT_EQ(info.locale, "pt-BR");
    EXPECT_EQ(info.languages, (std::vector<std::string>{"pt-BR", "en-US"}));
    EXPECT_EQ(info.timeZone, "America/Sao_Paulo");
}

TEST(AndroidDeviceInfoTest, TellsTheKindOfDeviceAndLeavesTheRestUnknown) {
    EXPECT_EQ(AndroidDeviceInfo::toSystemInfo(R"({"tablet": true})").deviceKind, SystemInfo::DeviceKind::Tablet);
    EXPECT_EQ(AndroidDeviceInfo::toSystemInfo(R"({"television": true, "tablet": true})").deviceKind, SystemInfo::DeviceKind::Tv);

    // The processor has no name before Android 12, and Java reports nothing when it fails.
    const SystemInfo empty = AndroidDeviceInfo::toSystemInfo("{}");
    EXPECT_EQ(empty.deviceKind, SystemInfo::DeviceKind::Phone);
    EXPECT_TRUE(empty.cpuName.empty());
    EXPECT_EQ(empty.memoryBytes, 0U);
    EXPECT_TRUE(empty.languages.empty());
    EXPECT_EQ(empty.toJson().dump(), R"({"deviceKind":"phone","languages":[],"os":"android"})");
}

} // namespace haylen::platform
