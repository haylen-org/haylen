#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "content/KeyStore.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::content {

class KeyStoreTest : public ::testing::Test {
  protected:
    [[nodiscard]] std::filesystem::path getFolder() const {
        return directory.getPath() / "keys" / "dev.haylen.tests";
    }

    test::TemporaryDirectory directory;
};

TEST_F(KeyStoreTest, CreatesKeysThatOnlyTheirOwnerReads) {
    const KeyStore created = KeyStore::create(getFolder(), "dev.haylen.tests");
    ASSERT_EQ(created.getContentKeyIds().size(), 1U);
    EXPECT_TRUE(created.getContentKeys()->contains(created.getActiveKeyId()));

#if !defined(_WIN32)
    using std::filesystem::perms;
    EXPECT_EQ(std::filesystem::status(getFolder()).permissions() & perms::all, perms::owner_all);
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(getFolder())) {
        EXPECT_EQ(entry.status().permissions() & perms::all, perms::owner_read | perms::owner_write) << entry.path();
    }
#endif

    const KeyStore opened = KeyStore::open(getFolder());
    EXPECT_EQ(opened.getIdentifier(), "dev.haylen.tests");
    EXPECT_EQ(opened.getContentKeyIds(), created.getContentKeyIds());
    EXPECT_EQ(opened.getSigningKey()->getVerifyingKey().getId(), created.getSigningKey()->getVerifyingKey().getId());
    EXPECT_THROW((void)KeyStore::create(getFolder(), "dev.haylen.tests"), std::invalid_argument) << "Existing keys are never replaced.";

    const KeyStore other = KeyStore::create(directory.getPath() / "other", "dev.haylen.tests");
    EXPECT_NE(other.getActiveKeyId(), created.getActiveKeyId()) << "Every key folder holds keys of its own.";
}

TEST_F(KeyStoreTest, RotatesContentKeysAndKeepsTheEarlierOnes) {
    KeyStore store = KeyStore::create(getFolder(), "dev.haylen.tests");
    const Digest first = store.getActiveKeyId();
    store.rotate();
    EXPECT_NE(store.getActiveKeyId(), first);

    const KeyStore opened = KeyStore::open(getFolder());
    ASSERT_EQ(opened.getContentKeyIds().size(), 2U);
    EXPECT_EQ(opened.getContentKeyIds().front(), first);
    EXPECT_EQ(opened.getActiveKeyId(), store.getActiveKeyId());
    EXPECT_TRUE(opened.getContentKeys()->contains(first));
}

TEST_F(KeyStoreTest, RefusesDamagedKeyFolders) {
    const KeyStore store = KeyStore::create(getFolder(), "dev.haylen.tests");
    const std::filesystem::path contentKey = getFolder() / ("content-" + store.getActiveKeyId().toHex() + ".key");
    EXPECT_THROW((void)KeyStore::open(directory.getPath() / "missing"), std::runtime_error);

    // A key file that holds another key than its name says is never used.
    std::filesystem::copy_file(getFolder() / KeyStore::kSigningKeyFile, contentKey, std::filesystem::copy_options::overwrite_existing);
    EXPECT_THROW((void)KeyStore::open(getFolder()), std::runtime_error);

    std::filesystem::resize_file(contentKey, 16);
    EXPECT_THROW((void)KeyStore::open(getFolder()), std::runtime_error);

    std::ofstream(getFolder() / KeyStore::kIndexFile) << "{\"identifier\": \"dev.haylen.tests\"}";
    EXPECT_THROW((void)KeyStore::open(getFolder()), std::runtime_error);
}

} // namespace haylen::content
