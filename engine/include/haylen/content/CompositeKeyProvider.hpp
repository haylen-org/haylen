#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "haylen/content/KeyProvider.hpp"

namespace haylen::content {

// The keys of several providers as one, in their order: the keys that a release build compiles in for the content that ships with the app and plays offline, next to a provider of an online game that obtains the keys of later content from its service. A key request goes to each provider in turn until one gives the key, so a service that fails never touches the keys the app already holds.
class CompositeKeyProvider final : public KeyProvider {
  public:
    explicit CompositeKeyProvider(std::vector<std::shared_ptr<const KeyProvider>> keyProviders);

    [[nodiscard]] std::vector<KeyId> getKeyIds() const override;
    [[nodiscard]] bool getKey(const KeyId& id, Key& target) const override;
    void requestKey(const KeyId& id, std::function<void(bool)> done) const override;

  private:
    using Providers = std::vector<std::shared_ptr<const KeyProvider>>;

    // A request that a service answers later keeps the providers alive, whatever happens to the composite meanwhile.
    static void requestFrom(std::shared_ptr<const Providers> list, std::size_t index, const KeyId& id, std::function<void(bool)> done);

    std::shared_ptr<const Providers> providers;
};

} // namespace haylen::content
