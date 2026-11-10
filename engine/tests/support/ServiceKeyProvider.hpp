#pragma once

#include <algorithm>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "content/crypto/ContentKey.hpp"
#include "haylen/content/KeyProvider.hpp"

namespace haylen::test {

// A key provider like the one of an online game, which holds no key until its service hands one out on request, from a thread of its own, and hands out only the keys a test allows. A test may hold the answers back until it releases them, so requests meet while the service is still asked.
class ServiceKeyProvider final : public content::KeyProvider {
  public:
    [[nodiscard]] static KeyId identify(const Key& key) {
        KeyId id{};
        std::ranges::copy(content::ContentKey(key).getId().getBytes(), id.begin());
        return id;
    }

    // The last holder of the provider may be a request that one of its answers ends, as the requests of a composite provider hold their providers, and a thread never waits for itself.
    ~ServiceKeyProvider() override {
        for (std::jthread& answer : answers) {
            if (answer.get_id() == std::this_thread::get_id()) {
                answer.detach();
            }
        }
    }

    ServiceKeyProvider() = default;
    ServiceKeyProvider(const ServiceKeyProvider&) = delete;
    ServiceKeyProvider& operator=(const ServiceKeyProvider&) = delete;

    void allow(const Key& key) {
        const std::scoped_lock lock(mutex);
        allowed.emplace(identify(key), key);
    }

    void holdAnswers() {
        const std::scoped_lock lock(mutex);
        holding = true;
    }

    void releaseAnswers() {
        {
            const std::scoped_lock lock(mutex);
            holding = false;
        }
        released.notify_all();
    }

    [[nodiscard]] int getRequests() const {
        const std::scoped_lock lock(mutex);
        return requests;
    }

    [[nodiscard]] std::vector<KeyId> getKeyIds() const override {
        const std::scoped_lock lock(mutex);
        std::vector<KeyId> ids;
        std::ranges::transform(held, std::back_inserter(ids), [](const auto& entry) { return entry.first; });
        return ids;
    }

    [[nodiscard]] bool getKey(const KeyId& id, Key& target) const override {
        const std::scoped_lock lock(mutex);
        const auto found = held.find(id);
        if (found == held.end()) {
            return false;
        }
        target = found->second;
        return true;
    }

    void requestKey(const KeyId& id, std::function<void(bool)> done) const override {
        const std::scoped_lock lock(mutex);
        ++requests;
        // clang-format off
        answers.emplace_back([this, id, done = std::move(done)] {
            bool given = false;
            {
                std::unique_lock answerLock(mutex);
                while (holding) {
                    released.wait(answerLock);
                }

                const auto found = allowed.find(id);
                if (found != allowed.end()) {
                    held.insert(*found);
                    given = true;
                }
            }
            done(given);
        });
        // clang-format on
    }

  private:
    mutable std::mutex mutex;
    std::map<KeyId, Key> allowed;
    mutable std::map<KeyId, Key> held;
    mutable int requests = 0;
    bool holding = false;
    mutable std::condition_variable released;

    // The answers that are still running end before the keys they reach go away.
    mutable std::vector<std::jthread> answers;
};

} // namespace haylen::test
