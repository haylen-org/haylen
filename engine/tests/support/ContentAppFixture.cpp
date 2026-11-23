#include "support/ContentAppFixture.hpp"
#include "haylen/lua/ScriptPlugin.hpp"

#include <lua.hpp>

#include <string_view>
#include <utility>

#include "content/ChannelPublisher.hpp"
#include "content/FolderStoreFiles.hpp"
#include "content/PublicationLayout.hpp"
#include "content/ReleaseInspector.hpp"
#include "content/ReleaseStager.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/lua/Application.hpp"
#include "support/LimitedStoreFiles.hpp"
#include "support/TestFiles.hpp"

namespace haylen::test {

ContentAppFixture::ContentAppFixture() {
    game = makeGame();
    release.build(io::MemoryPackage("source", game));
    publish(game);
}

ContentAppFixture::Files ContentAppFixture::makeGame() const {
    const std::string app = R"({"name": "Content", "identifier": "com.haylen.tests", "content": {"updates": {"baseUrl": ")" + server.getUrl() + R"(", "enabled": false}, "delivery": [{"match": "levels/**", "mode": "onDemand"}, {"match": "music/**", "mode": "prefetch"}]}})";
    Files files;
    files.emplace("haylen.json", TestFiles::bytes(app));
    files.emplace("source/main.lua", TestFiles::bytes("started = true\nrequire('haylen.events').on('contentRecovered', function(event) recovered = event.reason end)"));
    files.emplace("content/data/config.json", TestFiles::bytes(R"({"level": 1})"));
    files.emplace("content/levels/forest.json", TestFiles::bytes(R"({"trees": 12})"));
    files.emplace("content/music/theme.ogg", TestFiles::randomBytes(300 * 1024, 3));
    return files;
}

void ContentAppFixture::publish(const Files& files) const {
    const content::ChannelPublisher publisher(release.getKeys(), release.getContentKeyId(), release.getSharedSigningKey());
    (void)publisher.publish(io::MemoryPackage("source", files), tree.getPath(), {.profile = std::string(ReleaseFixture::kProfile), .appBuild = ReleaseFixture::kAppBuild, .channel = "stable", .release = release.getFolder()});
}

std::filesystem::path ContentAppFixture::shipWithoutContent() {
    const std::filesystem::path folder = shipped.getPath() / std::to_string(++ships);
    std::filesystem::create_directories(folder);

    for (const std::string_view manifest : {content::ReleasePackage::kAppManifestFile, content::ReleasePackage::kContentManifestFile}) {
        std::filesystem::copy_file(release.getFolder() / manifest, folder / manifest);
    }

    const content::ReleaseInspector inspector(release.getKeys(), {release.getSigningKey().getVerifyingKey()});
    for (const content::ShardReference& shard : inspector.inspect(release.getFolder()).app.envelope.shards) {
        std::filesystem::copy_file(release.getFolder() / shard.getFileName(), folder / shard.getFileName());
    }

    return folder;
}

void ContentAppFixture::setPacksStatus(int status) {
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(tree.getPath() / content::PublicationLayout::kPacksFolder)) {
        if (entry.is_regular_file()) {
            server.setStatus(entry.path().lexically_relative(tree.getPath()).generic_string(), status);
        }
    }
}

std::filesystem::path ContentAppFixture::ship() {
    const std::filesystem::path folder = shipped.getPath() / std::to_string(++ships);
    const content::ReleaseInspector inspector(release.getKeys(), {release.getSigningKey().getVerifyingKey()});
    (void)content::ReleaseStager(inspector, {}).stage(release.getFolder(), folder, std::nullopt, std::nullopt);
    return folder;
}

void ContentAppFixture::run(const std::filesystem::path& folder) {
    run(io::Package::openDirectory(folder));
}

void ContentAppFixture::run(std::shared_ptr<const io::Package> files) {
    engine.reset();
    const content::VerifyingKey trusted = release.getSigningKey().getVerifyingKey();
    std::unique_ptr<content::StoreFiles> storeFiles;
    if (storeRoom) {
        storeFiles = std::make_unique<LimitedStoreFiles>(storeFolder.getPath(), *storeRoom);
    } else {
        storeFiles = std::make_unique<content::FolderStoreFiles>(storeFolder.getPath());
    }

    auto store = storeRefused ? nullptr : std::make_shared<content::LocalStore>(std::move(storeFiles), release.getCompatibility().application, ReleaseFixture::kAppBuild);
    package = content::ReleasePackage::open(std::move(files), release.getKeys(), std::span(&trusted, 1), release.getCompatibility(), std::move(store));
    auto application = std::make_unique<lua::Application>();
    core::AppConfig config = core::AppConfig::fromPackage(*package, application->getSections());
    application->configure(config);
    engine = std::make_unique<core::Engine>(host, package, std::move(config), std::move(application));
    engine->start();
}

bool ContentAppFixture::frameUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }

        engine->frame(1.0 / 60.0);
    }

    return true;
}

bool ContentAppFixture::hasStarted() const {
    lua_State* L = engine->getPlugin<lua::ScriptPlugin>().getState();
    lua_getglobal(L, "started");
    const bool started = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return started;
}

std::string ContentAppFixture::getGlobalText(const char* name) const {
    lua_State* L = engine->getPlugin<lua::ScriptPlugin>().getState();
    lua_getglobal(L, name);
    std::string text = lua_isstring(L, -1) != 0 ? lua_tostring(L, -1) : "";
    lua_pop(L, 1);
    return text;
}

} // namespace haylen::test
