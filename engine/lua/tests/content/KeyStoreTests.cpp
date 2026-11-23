#include <gtest/gtest.h>

#include <exception>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>

#include "content/KeyStore.hpp"
#include "haylen/core/Json.hpp"
#include "support/PrivateFiles.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::content {

class KeyStoreTest : public ::testing::Test {
  protected:
    [[nodiscard]] std::filesystem::path getFolder() const {
        return directory.getPath() / "keys" / "com.haylen.tests";
    }

    static void expectError(const std::string& message, const std::function<void()>& action) {
        try {
            action();
            ADD_FAILURE() << "The action must fail with \"" << message << "\"";
        } catch (const std::exception& error) {
            EXPECT_EQ(std::string(error.what()), message);
        }
    }

    void writeIndex(const haylen::core::Json& index) const {
        test::TestFiles::write(getFolder() / haylen::content::KeyStore::kIndexFile, index.dump());
    }

    test::TemporaryDirectory directory;
};

TEST_F(KeyStoreTest, CreatesKeysThatOnlyTheirOwnerReads) {
    const haylen::content::KeyStore created = haylen::content::KeyStore::create(getFolder(), "com.haylen.tests");
    ASSERT_EQ(created.getContentKeyIds().size(), 1U);
    EXPECT_TRUE(created.getContentKeys()->contains(created.getActiveKeyId()));

    test::PrivateFiles::expectOwnerOnly(getFolder());

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(getFolder())) {
        test::PrivateFiles::expectOwnerOnly(entry.path());
    }

    const haylen::content::KeyStore opened = haylen::content::KeyStore::open(getFolder());
    EXPECT_EQ(opened.getIdentifier(), "com.haylen.tests");
    EXPECT_EQ(opened.getContentKeyIds(), created.getContentKeyIds());
    EXPECT_EQ(opened.getSigningKey()->getVerifyingKey().getId(), created.getSigningKey()->getVerifyingKey().getId());
    EXPECT_THROW((void)haylen::content::KeyStore::create(getFolder(), "com.haylen.tests"), std::invalid_argument) << "Existing keys are never replaced.";

    const haylen::content::KeyStore other = haylen::content::KeyStore::create(directory.getPath() / "other", "com.haylen.tests");
    EXPECT_NE(other.getActiveKeyId(), created.getActiveKeyId()) << "Every key folder holds keys of its own.";
    expectError("The keys of an app need the identifier of the app.", [&] { (void)haylen::content::KeyStore::create(directory.getPath() / "unnamed", ""); });
    EXPECT_FALSE(std::filesystem::exists(directory.getPath() / "unnamed"));
}

TEST_F(KeyStoreTest, RotatesContentKeysAndKeepsTheEarlierOnes) {
    haylen::content::KeyStore store = haylen::content::KeyStore::create(getFolder(), "com.haylen.tests");
    const haylen::content::Digest first = store.getActiveKeyId();
    store.rotate();
    EXPECT_NE(store.getActiveKeyId(), first);

    const haylen::content::KeyStore opened = haylen::content::KeyStore::open(getFolder());
    ASSERT_EQ(opened.getContentKeyIds().size(), 2U);
    EXPECT_EQ(opened.getContentKeyIds().front(), first);
    EXPECT_EQ(opened.getActiveKeyId(), store.getActiveKeyId());
    EXPECT_TRUE(opened.getContentKeys()->contains(first));
}

TEST_F(KeyStoreTest, RefusesDamagedKeyFolders) {
    const haylen::content::KeyStore store = haylen::content::KeyStore::create(getFolder(), "com.haylen.tests");
    const std::filesystem::path contentKey = getFolder() / ("content-" + store.getActiveKeyId().toHex() + ".key");
    EXPECT_THROW((void)haylen::content::KeyStore::open(directory.getPath() / "missing"), std::runtime_error);

    // An index that is no JSON, that names a content key by anything but its ID or that names another signing key than the folder holds is never used.
    const haylen::core::Json valid = haylen::core::Json::parse(test::TestFiles::readText(getFolder() / haylen::content::KeyStore::kIndexFile));
    const std::string damaged = "The key folder \"" + getFolder().generic_string() + "\" is damaged or was not made by Haylen. Restore it from a backup, since every release of the app needs its keys.";
    test::TestFiles::write(getFolder() / haylen::content::KeyStore::kIndexFile, "keys");
    expectError(damaged, [&] { (void)haylen::content::KeyStore::open(getFolder()); });
    haylen::core::Json changed = valid;
    changed["contentKeys"] = haylen::core::Json::array({"abc"});
    writeIndex(changed);
    expectError(damaged, [&] { (void)haylen::content::KeyStore::open(getFolder()); });
    changed = valid;
    changed["signingKey"] = store.getActiveKeyId().toHex();
    writeIndex(changed);
    expectError("The file \"" + (getFolder() / haylen::content::KeyStore::kSigningKeyFile).generic_string() + "\" holds another signing key than \"keys.json\" names.", [&] { (void)haylen::content::KeyStore::open(getFolder()); });
    writeIndex(valid);
    EXPECT_EQ(haylen::content::KeyStore::open(getFolder()).getContentKeyIds(), store.getContentKeyIds());

    // A key file that holds another key than its name says is never used.
    std::filesystem::copy_file(getFolder() / haylen::content::KeyStore::kSigningKeyFile, contentKey, std::filesystem::copy_options::overwrite_existing);
    EXPECT_THROW((void)haylen::content::KeyStore::open(getFolder()), std::runtime_error);

    // A key file shorter or longer than a key is never read as one.
    const std::string sizeError = "The key file \"" + contentKey.generic_string() + "\" is missing or does not hold the 32 bytes of a key.";
    std::filesystem::resize_file(contentKey, 33);
    expectError(sizeError, [&] { (void)haylen::content::KeyStore::open(getFolder()); });
    std::filesystem::resize_file(contentKey, 16);
    expectError(sizeError, [&] { (void)haylen::content::KeyStore::open(getFolder()); });

    test::TestFiles::write(getFolder() / haylen::content::KeyStore::kIndexFile, R"({"identifier": "com.haylen.tests"})");
    EXPECT_THROW((void)haylen::content::KeyStore::open(getFolder()), std::runtime_error);
}

} // namespace haylen::lua::content
