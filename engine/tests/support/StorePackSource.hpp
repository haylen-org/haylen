#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "haylen/content/Error.hpp"
#include "haylen/content/PackSource.hpp"

namespace haylen::test {

// A source of packs like the native plugin of a store, which delivers the packs that a test installed into folders, fails every request after `fail`, and keeps its answers for `answer` after `hold`, as a store that downloads a pack answers in a later frame.
class StorePackSource final : public content::PackSource {
  public:
    void install(const std::string& pack, std::filesystem::path folder) {
        folders[pack] = std::move(folder);
    }
    void fail(std::string message) {
        failure = std::move(message);
    }
    void hold() {
        holding = true;
    }
    void answer() {
        for (const std::function<void()>& respond : std::exchange(held, {})) {
            respond();
        }
    }
    [[nodiscard]] int getRequests() const noexcept {
        return requests;
    }

    void acquire(const std::string& pack, std::function<void(const Progress&)> progress, Delivery done) override {
        ++requests;
        progress({.receivedBytes = 1, .totalBytes = 1});
        // clang-format off
        auto respond = [this, pack, done = std::move(done)] {
            if (failure) {
                done(std::nullopt, content::Error(content::Error::Code::StoreAssetUnavailable, *failure));
                return;
            }
            const auto found = folders.find(pack);
            done(found != folders.end() ? std::optional(found->second) : std::nullopt, std::nullopt);
        };
        // clang-format on
        if (holding) {
            held.emplace_back(std::move(respond));
        } else {
            respond();
        }
    }

  private:
    std::map<std::string, std::filesystem::path> folders;
    std::optional<std::string> failure;
    std::vector<std::function<void()>> held;
    bool holding = false;
    int requests = 0;
};

} // namespace haylen::test
