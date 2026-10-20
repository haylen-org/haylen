#pragma once

#include <gtest/gtest.h>

#include <filesystem>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <aclapi.h>
#endif

namespace haylen::test {

// Checks that a file or folder opens for its owner alone: through its permissions on POSIX systems, and on Windows through an access list with one entry that the folders above it do not add to.
class PrivateFiles final {
  public:
    static void expectOwnerOnly(const std::filesystem::path& path) {
#if defined(_WIN32)
        PACL list = nullptr;
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        ASSERT_EQ(GetNamedSecurityInfoW(path.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &list, nullptr, &descriptor), static_cast<DWORD>(ERROR_SUCCESS)) << path;
        SECURITY_DESCRIPTOR_CONTROL control = 0;
        DWORD revision = 0;
        GetSecurityDescriptorControl(descriptor, &control, &revision);
        EXPECT_NE(control & SE_DACL_PROTECTED, 0) << path;
        EXPECT_EQ(list->AceCount, 1) << path;
        LocalFree(descriptor);
#else
        using std::filesystem::perms;
        const perms expected = std::filesystem::is_directory(path) ? perms::owner_all : perms::owner_read | perms::owner_write;
        EXPECT_EQ(std::filesystem::status(path).permissions() & perms::all, expected) << path;
#endif
    }
};

} // namespace haylen::test
