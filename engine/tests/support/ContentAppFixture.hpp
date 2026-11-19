#pragma once

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "content/ReleasePackage.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/io/Package.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/ContentServer.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::test {

// An app that runs a protected release with a content server: the required content that the app waits for, the files that download in the background or when the app asks for them, and the updates of its channel, all served by a local server.
class ContentAppFixture : public ::testing::Test {
  protected:
    using Files = std::map<std::string, std::vector<std::uint8_t>>;

    ContentAppFixture();

    [[nodiscard]] Files makeGame() const;
    void publish(const Files& files) const;

    // The release of an app on a platform that holds none of its content, such as a web page or a device that cleared its downloads: only the manifests and the app domain.
    [[nodiscard]] std::filesystem::path shipWithoutContent();

    // Makes the server answer every pack with a status, or serve them again with 0.
    void setPacksStatus(int status);

    // Lays out the release the way a platform ships it: what the app needs to start, and nothing that downloads.
    [[nodiscard]] std::filesystem::path ship();

    void run(const std::filesystem::path& folder);
    void run(std::shared_ptr<const io::Package> files);
    bool frameUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::seconds(20));
    [[nodiscard]] bool hasStarted() const;

    // The text of a global of the app, or nothing while it holds no text.
    [[nodiscard]] std::string getGlobalText(const char* name) const;

    TemporaryDirectory tree;
    ContentServer server{tree.getPath()};
    ReleaseFixture release;
    Files game;
    TemporaryDirectory shipped;
    TemporaryDirectory storeFolder;
    TemporaryDirectory data;
    platform::HeadlessHost host{data.getPath()};
    int ships = 0;
    std::optional<std::uint64_t> storeRoom;
    bool storeRefused = false;
    std::shared_ptr<content::ReleasePackage> package;
    std::unique_ptr<core::Engine> engine;
};

} // namespace haylen::test
