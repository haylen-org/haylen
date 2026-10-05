#include "support/ContentParsers.hpp"

#include <vector>

#include "content/Error.hpp"
#include "content/crypto/SigningKey.hpp"
#include "content/format/Catalog.hpp"
#include "content/format/ChannelDescriptor.hpp"
#include "content/format/ChunkRecord.hpp"
#include "content/format/Manifest.hpp"
#include "content/format/ShardHeader.hpp"
#include "content/format/ShardIndex.hpp"
#include "support/ReleaseFixture.hpp"

namespace haylen::test {

const std::vector<content::VerifyingKey>& ContentParsers::getTrustedKeys() {
    static const std::vector<content::VerifyingKey> trusted{content::SigningKey(ReleaseFixture::makeKey(101)).getVerifyingKey()};
    return trusted;
}

const content::KeyRing& ContentParsers::getKeys() {
    // clang-format off
    static const content::KeyRing keys = [] {
        content::KeyRing ring;
        ring.add(ReleaseFixture::makeKey(1));
        return ring;
    }();
    // clang-format on
    return keys;
}

void ContentParsers::parse(std::span<const std::uint8_t> input) {
    if (input.empty()) {
        return;
    }

    const std::span<const std::uint8_t> bytes = input.subspan(1);
    try {
        switch (input.front() % 6) {
        case 0:
            if (bytes.size() >= content::ChunkRecord::kHeaderSize) {
                (void)content::ChunkRecord::parse(bytes.first<content::ChunkRecord::kHeaderSize>());
            }
            break;
        case 1:
            if (bytes.size() >= content::ShardHeader::kSize) {
                (void)content::ShardHeader::parse(bytes.first<content::ShardHeader::kSize>(), bytes.size());
            }
            break;
        case 2:
            (void)content::ShardIndex::parse(bytes, content::ShardHeader::kSize, std::uint64_t{1} << 40);
            break;
        case 3: {
            const content::Catalog catalog = content::Catalog::parse({bytes.begin(), bytes.end()}, 4);
            for (std::uint64_t index = 0; index < catalog.getFileCount(); ++index) {
                const content::Catalog::File file = catalog.getFile(index);
                (void)catalog.findFile(file.path);
                (void)catalog.findFolder(file.path);
                if (file.size > 0) {
                    (void)catalog.findPart(file, file.size - 1);
                }
            }
            break;
        }
        case 4:
            (void)content::Manifest::read({bytes.begin(), bytes.end()}, getTrustedKeys()).decryptCatalog(getKeys());
            break;
        default:
            (void)content::ChannelDescriptor::read(bytes, getTrustedKeys());
            break;
        }
    } catch (const content::Error&) {}
}

} // namespace haylen::test
