#include <gtest/gtest.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>

#include "content/KeyStore.hpp"
#include "haylen/core/Json.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::content {

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

    void writeIndex(const core::Json& index) const {
        std::ofstream(getFolder() / KeyStore::kIndexFile, std::ios::trunc) << index.dump();
    }

    test::TemporaryDirectory directory;
};

TEST_F(KeyStoreTest, CreatesKeysThatOnlyTheirOwnerReads) {
    const KeyStore created = KeyStore::create(getFolder(), "com.haylen.tests");
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
    EXPECT_EQ(opened.getIdentifier(), "com.haylen.tests");
    EXPECT_EQ(opened.getContentKeyIds(), created.getContentKeyIds());
    EXPECT_EQ(opened.getSigningKey()->getVerifyingKey().getId(), created.getSigningKey()->getVerifyingKey().getId());
    EXPECT_THROW((void)KeyStore::create(getFolder(), "com.haylen.tests"), std::invalid_argument) << "Existing keys are never replaced.";

    const KeyStore other = KeyStore::create(directory.getPath() / "other", "com.haylen.tests");
    EXPECT_NE(other.getActiveKeyId(), created.getActiveKeyId()) << "Every key folder holds keys of its own.";
    expectError("The keys of an app need the identifier of the app.", [&] { (void)KeyStore::create(directory.getPath() / "unnamed", ""); });
    EXPECT_FALSE(std::filesystem::exists(directory.getPath() / "unnamed"));
}

TEST_F(KeyStoreTest, RotatesContentKeysAndKeepsTheEarlierOnes) {
    KeyStore store = KeyStore::create(getFolder(), "com.haylen.tests");
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
    const KeyStore store = KeyStore::create(getFolder(), "com.haylen.tests");
    const std::filesystem::path contentKey = getFolder() / ("content-" + store.getActiveKeyId().toHex() + ".key");
    EXPECT_THROW((void)KeyStore::open(directory.getPath() / "missing"), std::runtime_error);

    // An index that is no JSON, that names a content key by anything but its ID or that names another signing key than the folder holds is never used.
    std::ifstream stream(getFolder() / KeyStore::kIndexFile);
    const core::Json valid = core::Json::parse(stream);
    stream.close();
    const std::string index = (getFolder() / KeyStore::kIndexFile).generic_string();
    std::ofstream(getFolder() / KeyStore::kIndexFile, std::ios::trunc) << "keys";
    expectError("The file \"" + index + "\" is not the JSON index of a key folder.", [&] { (void)KeyStore::open(getFolder()); });
    core::Json changed = valid;
    changed["contentKeys"] = core::Json::array({"abc"});
    writeIndex(changed);
    expectError("The file \"" + index + "\" lists a content key ID that is not 64 hexadecimal digits.", [&] { (void)KeyStore::open(getFolder()); });
    changed = valid;
    changed["signingKey"] = store.getActiveKeyId().toHex();
    writeIndex(changed);
    expectError("The file \"" + (getFolder() / KeyStore::kSigningKeyFile).generic_string() + "\" holds another signing key than \"keys.json\" names.", [&] { (void)KeyStore::open(getFolder()); });
    writeIndex(valid);
    EXPECT_EQ(KeyStore::open(getFolder()).getContentKeyIds(), store.getContentKeyIds());

    // A key file that holds another key than its name says is never used.
    std::filesystem::copy_file(getFolder() / KeyStore::kSigningKeyFile, contentKey, std::filesystem::copy_options::overwrite_existing);
    EXPECT_THROW((void)KeyStore::open(getFolder()), std::runtime_error);

    std::filesystem::resize_file(contentKey, 16);
    EXPECT_THROW((void)KeyStore::open(getFolder()), std::runtime_error);

    std::ofstream(getFolder() / KeyStore::kIndexFile) << "{\"identifier\": \"com.haylen.tests\"}";
    EXPECT_THROW((void)KeyStore::open(getFolder()), std::runtime_error);
}

} // namespace haylen::content
