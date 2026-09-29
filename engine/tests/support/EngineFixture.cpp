#include "support/EngineFixture.hpp"

#include <lua.hpp>

#include <stdexcept>
#include <thread>

#include "haylen/io/MemoryPackage.hpp"
#include "support/TestFiles.hpp"

namespace haylen::test {

EngineFixture::EngineFixture(std::map<std::string, std::string> files, std::unique_ptr<core::Application> application) : headlessHost(directory.getPath() / "data") {
    files.try_emplace("app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests"})");
    files.try_emplace("source/main.lua", "");

    std::map<std::string, std::vector<std::uint8_t>> contents;
    for (const auto& [path, text] : files) {
        contents.emplace(path, TestFiles::bytes(text));
    }
    memoryPackage = std::make_shared<io::MemoryPackage>("test", std::move(contents));
    launch(std::move(application));
}

EngineFixture::~EngineFixture() = default;

void EngineFixture::launch(std::unique_ptr<core::Application> application) {
    core::AppConfig config = core::AppConfig::fromPackage(*memoryPackage);
    if (!application) {
        application = std::make_unique<lua::Application>();
    }
    application->configure(config);
    headlessHost.setTransparencySupported(config.window.transparent);
    runningEngine = std::make_unique<core::Engine>(headlessHost, memoryPackage, std::move(config), std::move(application));
    runningEngine->start();
}

void EngineFixture::restart() {
    runningEngine.reset();
    launch(nullptr);
}

void EngineFixture::frames(int count, double seconds) {
    for (int frame = 0; frame < count; ++frame) {
        runningEngine->frame(seconds);
    }
}

bool EngineFixture::frameUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        runningEngine->frame(1.0 / 60.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

std::string EngineFixture::lua(const std::string& source) {
    lua_State* L = runningEngine->getLuaState();
    const int top = lua_gettop(L);
    std::string result;
    if (luaL_loadbufferx(L, source.data(), source.size(), "=test", "t") != LUA_OK || lua_pcall(L, 0, 1, 0) != LUA_OK) {
        result = std::string("error: ") + lua_tostring(L, -1);
    } else {
        result = luaL_tolstring(L, -1, nullptr);
    }
    lua_settop(L, top);
    return result;
}

void EngineFixture::runLua(const std::string& source) {
    const std::string result = lua(source);
    if (result.starts_with("error: ")) {
        throw std::runtime_error(result);
    }
}

} // namespace haylen::test
