#include "content/KeyStore.hpp"

#include <monocypher.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <aclapi.h>
#endif

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

#include "content/SecureRandom.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::content {

KeyStore::KeyStore(std::filesystem::path keyFolder, std::string appIdentifier) : folder(std::move(keyFolder)), identifier(std::move(appIdentifier)) {}

std::string KeyStore::getContentKeyFile(const Digest& id) {
    return "content-" + id.toHex() + ".key";
}

KeyStore KeyStore::create(const std::filesystem::path& folder, std::string identifier) {
    if (identifier.empty()) {
        throw std::invalid_argument("The keys of an app need the identifier of the app.");
    }
    if (std::filesystem::exists(folder / kIndexFile)) {
        throw std::invalid_argument("The key folder \"" + folder.generic_string() + "\" holds keys already, and they stay, since every release of the app needs them.");
    }

    // The folder is closed to everyone but its owner before any key reaches it.
    std::filesystem::create_directories(folder);
    restrictToOwner(folder, true);

    KeyStore store(folder, std::move(identifier));
    Secret seed{};
    SecureRandom::fill(seed);
    writePrivate(folder / kSigningKeyFile, seed);
    store.signingKey = std::make_shared<const SigningKey>(seed);
    crypto_wipe(seed.data(), seed.size());
    store.rotate();
    return store;
}

KeyStore KeyStore::open(const std::filesystem::path& folder) {
    std::ifstream stream(folder / kIndexFile);
    if (!stream) {
        throw std::runtime_error("The key folder \"" + folder.generic_string() + "\" holds no \"" + std::string(kIndexFile) + "\".");
    }
    core::Json index;
    try {
        index = core::Json::parse(stream);
    } catch (const core::Json::exception&) {
        throw std::runtime_error("The file \"" + (folder / kIndexFile).generic_string() + "\" is not the JSON index of a key folder.");
    }
    if (!index.is_object() || !index.contains("identifier") || !index["identifier"].is_string() || !index.contains("contentKeys") || !index["contentKeys"].is_array() || index["contentKeys"].empty() || !index.contains("signingKey") || !index["signingKey"].is_string()) {
        throw std::runtime_error("The file \"" + (folder / kIndexFile).generic_string() + "\" needs the \"identifier\" of the app, its \"contentKeys\" and its \"signingKey\".");
    }

    KeyStore store(folder, index["identifier"].get<std::string>());
    for (const core::Json& entry : index["contentKeys"]) {
        const std::optional<Digest> id = entry.is_string() ? Digest::fromHex(entry.get<std::string>()) : std::nullopt;
        if (!id) {
            throw std::runtime_error("The file \"" + (folder / kIndexFile).generic_string() + "\" lists a content key ID that is not 64 hexadecimal digits.");
        }
        Secret master = readSecret(folder / getContentKeyFile(*id));
        if (store.ring->add(master) != *id) {
            crypto_wipe(master.data(), master.size());
            throw std::runtime_error("The file \"" + (folder / getContentKeyFile(*id)).generic_string() + "\" holds another key than its name says.");
        }
        crypto_wipe(master.data(), master.size());
        store.contentKeyIds.push_back(*id);
    }

    Secret seed = readSecret(folder / kSigningKeyFile);
    store.signingKey = std::make_shared<const SigningKey>(seed);
    crypto_wipe(seed.data(), seed.size());
    if (store.signingKey->getVerifyingKey().getId().toHex() != index["signingKey"].get<std::string>()) {
        throw std::runtime_error("The file \"" + (folder / kSigningKeyFile).generic_string() + "\" holds another signing key than \"" + std::string(kIndexFile) + "\" names.");
    }
    return store;
}

void KeyStore::rotate() {
    Secret master{};
    SecureRandom::fill(master);
    const Digest& id = ring->add(master);
    writePrivate(folder / getContentKeyFile(id), master);
    crypto_wipe(master.data(), master.size());
    contentKeyIds.push_back(id);
    writeIndex();
}

std::vector<EmbeddedKeyProvider::SealedKey> KeyStore::sealContentKeys() const {
    std::vector<EmbeddedKeyProvider::SealedKey> sealed;
    for (const Digest& id : contentKeyIds) {
        Secret master = readSecret(folder / getContentKeyFile(id));
        EmbeddedKeyProvider::KeyId keyId{};
        std::ranges::copy(id.getBytes(), keyId.begin());
        sealed.push_back(EmbeddedKeyProvider::seal(keyId, master));
        crypto_wipe(master.data(), master.size());
    }
    return sealed;
}

KeyStore::Secret KeyStore::readSecret(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (!stream.is_open() || bytes.size() != std::tuple_size_v<Secret>) {
        crypto_wipe(bytes.data(), bytes.size());
        throw std::runtime_error("The key file \"" + file.generic_string() + "\" is missing or does not hold the 32 bytes of a key.");
    }
    Secret secret{};
    std::ranges::copy(bytes, secret.begin());
    crypto_wipe(bytes.data(), bytes.size());
    return secret;
}

void KeyStore::writePrivate(const std::filesystem::path& file, std::span<const std::uint8_t> bytes) {
    std::ofstream(file, std::ios::binary | std::ios::trunc).close();
    restrictToOwner(file, false);
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream) {
        throw std::runtime_error("The private file \"" + file.generic_string() + "\" could not be written.");
    }
}

void KeyStore::restrictToOwner(const std::filesystem::path& path, bool folder) {
#if defined(_WIN32)
    // Windows keeps who may open a file in its access list, which then grants the user who runs the tool alone, without the entries that the folders above pass down. The rights are those of files, since generic rights that a folder passes down become a second entry.
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) == FALSE) {
        throw std::runtime_error("The user that runs the tool could not be identified to close its private files to everyone else.");
    }
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<std::uint8_t> user(size);
    const BOOL identified = GetTokenInformation(token, TokenUser, user.data(), size, &size);
    CloseHandle(token);
    if (identified == FALSE) {
        throw std::runtime_error("The user that runs the tool could not be identified to close its private files to everyone else.");
    }

    EXPLICIT_ACCESS_W access{};
    access.grfAccessPermissions = FILE_ALL_ACCESS;
    access.grfAccessMode = SET_ACCESS;
    access.grfInheritance = folder ? SUB_CONTAINERS_AND_OBJECTS_INHERIT : NO_INHERITANCE;
    access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    access.Trustee.TrusteeType = TRUSTEE_IS_USER;
    access.Trustee.ptstrName = static_cast<LPWSTR>(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid);
    PACL list = nullptr;
    if (SetEntriesInAclW(1, &access, nullptr, &list) != ERROR_SUCCESS) {
        throw std::runtime_error("The access list that closes \"" + path.generic_string() + "\" to everyone but its owner could not be made.");
    }
    std::wstring name = path.wstring();
    const DWORD result = SetNamedSecurityInfoW(name.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, list, nullptr);
    LocalFree(list);
    if (result != ERROR_SUCCESS) {
        throw std::runtime_error("The file \"" + path.generic_string() + "\" could not be closed to everyone but its owner.");
    }
#else
    using std::filesystem::perms;
    std::filesystem::permissions(path, folder ? perms::owner_all : perms::owner_read | perms::owner_write, std::filesystem::perm_options::replace);
#endif
}

void KeyStore::writeIndex() const {
    core::Json ids = core::Json::array();
    for (const Digest& id : contentKeyIds) {
        ids.push_back(id.toHex());
    }
    const core::Json index = {{"identifier", identifier}, {"contentKeys", ids}, {"signingKey", signingKey->getVerifyingKey().getId().toHex()}};

    // The index takes its name only once it is complete, so an interrupted rotation keeps the keys it had.
    std::filesystem::path partial = folder / kIndexFile;
    partial += ".partial";
    const std::string text = index.dump(4) + "\n";
    writePrivate(partial, {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
    std::filesystem::rename(partial, folder / kIndexFile);
}

} // namespace haylen::content
