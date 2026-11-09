#include <gtest/gtest.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif !defined(__EMSCRIPTEN__)
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "platform/CrashGuard.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::platform {

class CrashGuardTest : public ::testing::Test {
  protected:
    static constexpr int kForwarded = 3;
    static constexpr int kForwardedWithoutTheCrash = 4;

    void TearDown() override {
        CrashGuard::setReport({});
    }

    [[nodiscard]] static core::Json readReport(const std::filesystem::path& file) {
        std::ifstream stream(file, std::ios::binary);
        return core::Json::parse(std::string(std::istreambuf_iterator<char>(stream), {}));
    }

#if defined(_WIN32)
    static long __stdcall exitFromThePreviousFilter(_EXCEPTION_POINTERS* info) {
        _exit(info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ? kForwarded : kForwardedWithoutTheCrash);
    }
#elif !defined(__EMSCRIPTEN__)
    // The fault of a page without access arrives as a segmentation fault on Linux and as a bus error on Apple platforms.
    static void installPreviousHandler(void (*handler)(int, siginfo_t*, void*)) {
        struct sigaction action{};
        action.sa_sigaction = handler;
        action.sa_flags = SA_SIGINFO;
        sigemptyset(&action.sa_mask);
        sigaction(SIGSEGV, &action, nullptr);
        sigaction(SIGBUS, &action, nullptr);
    }

    static void exitWithTheFaultAddress(int, siginfo_t* info, void*) {
        _exit(info->si_addr == faultAddress ? kForwarded : kForwardedWithoutTheCrash);
    }

    static void reportLaterAndExit(int signal, siginfo_t* info, void* context) {
        guardAction.sa_sigaction(signal, info, context);
        _exit(kForwarded);
    }

    static void touchAPageWithoutAccess() {
        void* page = mmap(nullptr, static_cast<std::size_t>(sysconf(_SC_PAGESIZE)), PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
        ASSERT_NE(page, MAP_FAILED);
        faultAddress = page;
        *static_cast<volatile int*>(page) = 1;
    }

    static inline void* faultAddress = nullptr;
    static inline struct sigaction guardAction{};
#endif

    test::TemporaryDirectory directory;
};

TEST_F(CrashGuardTest, WritesTheReasonAndTheLastPublishedContext) {
    const std::filesystem::path report = directory.getPath() / "crash-report.json";
    CrashGuard::writeReport("segmentationFault");
    EXPECT_FALSE(std::filesystem::exists(report)) << "Nothing is written before the guard knows its file.";

    CrashGuard::setReport(report);
    CrashGuard::setContext(R"({"breadcrumbs":[{"message":"Opened the shop."}]})");
    CrashGuard::setContext(R"({"breadcrumbs":[{"message":"Bought a sword."}]})");
    CrashGuard::writeReport("segmentationFault");
    const core::Json written = readReport(report);
    EXPECT_EQ(written.at("reason"), "segmentationFault");
    EXPECT_EQ(written.at("context").at("breadcrumbs").at(0).at("message"), "Bought a sword.");

    CrashGuard::setContext(std::string(CrashGuard::kContextSize + 1, ' '));
    CrashGuard::writeReport("abort");
    EXPECT_EQ(readReport(report), (core::Json{{"reason", "abort"}, {"context", core::Json::object()}}));
}

#if defined(_WIN32)
TEST_F(CrashGuardTest, HandsAnExceptionToTheFilterThatWasThereBefore) {
    // clang-format off
    EXPECT_EXIT({
        SetUnhandledExceptionFilter(&exitFromThePreviousFilter);
        CrashGuard::install();
        EXCEPTION_RECORD record{.ExceptionCode = EXCEPTION_ACCESS_VIOLATION};
        CONTEXT context{};
        EXCEPTION_POINTERS pointers{.ExceptionRecord = &record, .ContextRecord = &context};
        UnhandledExceptionFilter(&pointers);
    }, testing::ExitedWithCode(kForwarded), "");
    // clang-format on
}
#elif !defined(__EMSCRIPTEN__)
// A crash reporter that a plugin installed before the guard receives the fault itself, with its address, not a signal that the guard raised again.
TEST_F(CrashGuardTest, HandsAFaultWithItsInformationToTheHandlerThatWasThereBefore) {
    GTEST_FLAG_SET(death_test_style, "fast");
    const std::filesystem::path report = directory.getPath() / "crash-report.json";
    // clang-format off
    EXPECT_EXIT({
        installPreviousHandler(&exitWithTheFaultAddress);
        CrashGuard::install();
        CrashGuard::setReport(report);
        touchAPageWithoutAccess();
    }, testing::ExitedWithCode(kForwarded), "");
    // clang-format on
    const std::string reason = readReport(report).at("reason");
    EXPECT_TRUE(reason == "segmentationFault" || reason == "busError") << reason;
}

// A crash reporter that a plugin installs after the guard sees the crash first and hands it on to the guard, which writes its report.
TEST_F(CrashGuardTest, WritesTheReportWhenAHandlerInstalledLaterHandsTheCrashOn) {
    GTEST_FLAG_SET(death_test_style, "fast");
    const std::filesystem::path report = directory.getPath() / "crash-report.json";
    // clang-format off
    EXPECT_EXIT({
        CrashGuard::install();
        CrashGuard::setReport(report);
        struct sigaction action{};
        action.sa_sigaction = &reportLaterAndExit;
        action.sa_flags = SA_SIGINFO;
        sigemptyset(&action.sa_mask);
        sigaction(SIGABRT, &action, &guardAction);
        raise(SIGABRT);
    }, testing::ExitedWithCode(kForwarded), "");
    // clang-format on
    EXPECT_EQ(readReport(report).at("reason"), "abort");
}
#endif

TEST(CrashLuaTest, HandsTheReportOfALastCrashToTheNextStartAndKeepsBreadcrumbs) {
    test::EngineFixture fixture;
    storage::UserStorage& files = fixture.engine().getStorage();
    EXPECT_EQ(fixture.host().getCrashReport(), files.getRoot() / "crash-report.json");
    EXPECT_EQ(fixture.lua("return tostring(require('haylen.debug').lastCrash())"), "nil");

    // clang-format off
    fixture.runLua(R"(
        debugging = require('haylen.debug')
        heard = {}
        require('haylen.events').on('breadcrumbAdded', function(crumb) heard[#heard + 1] = crumb.message .. ' ' .. crumb.data.items end)
        debugging.addBreadcrumb('Opened the shop.', {items = 12})
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return heard[1]"), "Opened the shop. 12");
    const core::Json context = core::Json::parse(fixture.host().getCrashContext());
    EXPECT_EQ(context.at("breadcrumbs").back().at("message"), "Opened the shop.");
    EXPECT_EQ(context.at("app").at("identifier"), fixture.engine().getConfig().identifier);
    EXPECT_EQ(context.at("app").at("build"), fixture.engine().getConfig().build);
    EXPECT_TRUE(context.at("log").is_array());

    fixture.runLua("for index = 1, 150 do debugging.addBreadcrumb('Step ' .. index) end");
    EXPECT_EQ(fixture.lua("local kept = debugging.breadcrumbs() return #kept .. ' ' .. kept[1].message .. ' ' .. kept[#kept].message"), "100 Step 51 Step 150");
    fixture.frames(1);
    EXPECT_EQ(core::Json::parse(fixture.host().getCrashContext()).at("breadcrumbs").size(), 30U);

    files.writeText("crash-report.json", R"({"reason":"segmentationFault","context":{"breadcrumbs":[{"message":"Bought a sword."}]}})");
    fixture.restart();
    EXPECT_EQ(fixture.lua("local crash = require('haylen.debug').lastCrash() return crash.reason .. ' ' .. crash.context.breadcrumbs[1].message"), "segmentationFault Bought a sword.");
    EXPECT_FALSE(fixture.engine().getStorage().exists("crash-report.json"));
    fixture.restart();
    EXPECT_EQ(fixture.lua("return tostring(require('haylen.debug').lastCrash())"), "nil");
}

} // namespace haylen::platform
