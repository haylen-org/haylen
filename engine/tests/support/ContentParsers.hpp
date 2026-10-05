#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "content/crypto/KeyRing.hpp"
#include "content/crypto/VerifyingKey.hpp"

namespace haylen::test {

// Feeds bytes to the parsers of the content formats, which treat every file as hostile: the first byte picks the parser and the rest is its input. A parser may reject the input with a content error, and anything else, a crash or another exception, is a bug. The fuzzer of the content formats and the hostile input tests both drive it.
class ContentParsers final {
  public:
    static void parse(std::span<const std::uint8_t> input);

  private:
    // The keys of the release fixture, so signed inputs of the tests reach the parsers behind the signature.
    [[nodiscard]] static const std::vector<content::VerifyingKey>& getTrustedKeys();
    [[nodiscard]] static const content::KeyRing& getKeys();
};

} // namespace haylen::test
