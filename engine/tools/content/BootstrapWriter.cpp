#include "content/BootstrapWriter.hpp"

#include <format>
#include <vector>

#include "content/crypto/VerifyingKey.hpp"

namespace haylen::content {

std::string BootstrapWriter::writeBytes(std::span<const std::uint8_t> bytes) {
    std::string text = "{";
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        text += std::format("{}0x{:02x}", index == 0 ? "" : ", ", bytes[index]);
    }
    return text + "}";
}

// Escapes every character that could end the literal or that is not printable ASCII, with octal escapes, which never run into the characters after them.
std::string BootstrapWriter::writeText(std::string_view text) {
    std::string literal = "\"";
    for (const char character : text) {
        const auto code = static_cast<unsigned char>(character);
        if (character == '"' || character == '\\' || code < 0x20 || code > 0x7E) {
            literal += std::format("\\{:03o}", code);
        } else {
            literal += character;
        }
    }
    return literal + "\"";
}

std::string BootstrapWriter::write(const KeyStore& keys, std::string_view profile, std::uint64_t appBuild) {
    std::string sealed;
    for (const EmbeddedKeyProvider::SealedKey& key : keys.sealContentKeys()) {
        sealed += std::format("        {{.id = {}, .mask = {}, .sealed = {}}},\n", writeBytes(key.id), writeBytes(key.mask), writeBytes(key.sealed));
    }
    const VerifyingKey& verifying = keys.getSigningKey()->getVerifyingKey();
    return std::format(R"(// Written by haylen.py from the key folder of the app for its release builds. It opens the protected release of the app, holds no key in plain form and never belongs in a repository.
#include <memory>
#include <utility>
#include <vector>

#include "haylen/content/Bootstrap.hpp"
#include "haylen/content/EmbeddedKeyProvider.hpp"

class HaylenBootstrap final {{
  public:
    static const bool kInstalled;

  private:
    static bool install();
}};

bool HaylenBootstrap::install() {{
    std::vector<haylen::content::EmbeddedKeyProvider::SealedKey> keys = {{
{}    }};
    return haylen::content::Bootstrap::install({{.identifier = {}, .appBuild = {}U, .profile = {}, .trustedKeys = {{{}}}, .keys = std::make_shared<const haylen::content::EmbeddedKeyProvider>(std::move(keys))}});
}}

const bool HaylenBootstrap::kInstalled = HaylenBootstrap::install();
)",
                       sealed, writeText(keys.getIdentifier()), appBuild, writeText(profile), writeBytes(verifying.getBytes()));
}

} // namespace haylen::content
