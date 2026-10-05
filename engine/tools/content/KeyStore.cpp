#include "content/KeyStore.hpp"

#include <monocypher.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <utility>

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
    std::filesystem::permissions(folder, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);

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
    // The file is closed to everyone but its owner before anything is written into it.
    std::ofstream(file, std::ios::binary | std::ios::trunc).close();
    std::filesystem::permissions(file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream) {
        throw std::runtime_error("The file \"" + file.generic_string() + "\" of the key folder could not be written.");
    }
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
