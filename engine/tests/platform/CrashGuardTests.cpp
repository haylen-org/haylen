#include <gtest/gtest.h>

#if defined(_WIN32)
#include <windows.h>
#elif !defined(__EMSCRIPTEN__)
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>

#include "haylen/core/Json.hpp"
#include "platform/CrashGuard.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::platform {

class CrashGuardTest : public ::testing::Test {
  protected:
    static constexpr int kForwarded = 3;
    static constexpr int kForwardedWithoutTheCrash = 4;

    void TearDown() override {
        CrashGuard::setReport({});
    }

    [[nodiscard]] static haylen::core::Json readReport(const std::filesystem::path& file) {
        return haylen::core::Json::parse(test::TestFiles::readText(file));
    }

#if defined(_WIN32)
    static long __stdcall exitFromThePreviousFilter(_EXCEPTION_POINTERS* info) {
        _exit(info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ? kForwarded : kForwardedWithoutTheCrash);
    }

    // Hands an access violation to the filter of the process, as a crash does.
    static void crashThroughTheFilter() {
        SetUnhandledExceptionFilter(&exitFromThePreviousFilter);
        CrashGuard::install();
        EXCEPTION_RECORD record{.ExceptionCode = EXCEPTION_ACCESS_VIOLATION};
        CONTEXT context{};
        EXCEPTION_POINTERS pointers{.ExceptionRecord = &record, .ContextRecord = &context};
        UnhandledExceptionFilter(&pointers);
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

    static void exitAfterTheCrash(int, siginfo_t*, void*) {
        _exit(kForwarded);
    }

    static int overflowTheStack(int depth) {
        std::array<volatile char, 4096> frame{};
        frame[0] = static_cast<char>(depth);

        if (depth < 0) {
            return 0;
        }

        return overflowTheStack(depth + 1) + frame[static_cast<std::size_t>(depth) % frame.size()];
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
    const haylen::core::Json written = readReport(report);
    EXPECT_EQ(written.at("reason"), "segmentationFault");
    EXPECT_EQ(written.at("context").at("breadcrumbs").at(0).at("message"), "Bought a sword.");

    CrashGuard::setContext(std::string(CrashGuard::kContextSize + 1, ' '));
    CrashGuard::writeReport("abort");
    EXPECT_EQ(readReport(report), (haylen::core::Json{{"reason", "abort"}, {"context", haylen::core::Json::object()}}));
}

#if defined(_WIN32)
TEST_F(CrashGuardTest, HandsAnExceptionToTheFilterThatWasThereBefore) {
    EXPECT_EXIT(crashThroughTheFilter(), testing::ExitedWithCode(kForwarded), "");
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

// The thread that installs the guard plays the frames, and a stack overflow there leaves no stack for a handler but its alternate one.
TEST_F(CrashGuardTest, ReportsAStackOverflow) {
    GTEST_FLAG_SET(death_test_style, "fast");
    const std::filesystem::path report = directory.getPath() / "crash-report.json";
    // clang-format off
    EXPECT_EXIT({
        installPreviousHandler(&exitAfterTheCrash);
        CrashGuard::install();
        CrashGuard::setReport(report);
        overflowTheStack(0);
    }, testing::ExitedWithCode(kForwarded), "");
    // clang-format on
    const std::string reason = readReport(report).at("reason");
    EXPECT_TRUE(reason == "segmentationFault" || reason == "busError") << reason;
}
#endif

} // namespace haylen::platform
