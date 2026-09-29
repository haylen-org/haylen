#include "support/EngineFixture.hpp"

#include <lua.hpp>

#include <stdexcept>
#include <thread>

#include "haylen/core/Json.hpp"
#include "haylen/io/MemoryPackage.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace haylen::test {

EngineFixture::EngineFixture(std::map<std::string, std::string> files, std::unique_ptr<core::Application> application) : headlessHost(directory.getPath() / "data") {
    files.try_emplace("app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests"})");
    files.try_emplace("source/main.lua", "");

    std::map<std::string, std::vector<std::uint8_t>> contents;
    for (const auto& [path, text] : files) {
        contents.emplace(path, bytes(text));
    }
    memoryPackage = std::make_shared<io::MemoryPackage>("test", std::move(contents));

    core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(memoryPackage->readText("app.json")));
    if (!application) {
        application = std::make_unique<lua::Application>();
    }
    application->configure(config);
    runningEngine = std::make_unique<core::Engine>(headlessHost, memoryPackage, std::move(config), std::move(application));
    runningEngine->start();
}

EngineFixture::~EngineFixture() = default;

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

std::vector<std::uint8_t> bytes(const std::string& text) {
    return {text.begin(), text.end()};
}

std::vector<std::uint8_t> pngImage(int width, int height, std::uint32_t rgba) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset + 0] = static_cast<std::uint8_t>((rgba >> 24U) & 0xFFU);
        pixels[offset + 1] = static_cast<std::uint8_t>((rgba >> 16U) & 0xFFU);
        pixels[offset + 2] = static_cast<std::uint8_t>((rgba >> 8U) & 0xFFU);
        pixels[offset + 3] = static_cast<std::uint8_t>(rgba & 0xFFU);
    }

    std::vector<std::uint8_t> encoded;
    // clang-format off
    stbi_write_png_to_func([](void* context, void* data, int size) {
        auto* output = static_cast<std::vector<std::uint8_t>*>(context);
        const auto* begin = static_cast<const std::uint8_t*>(data);
        output->insert(output->end(), begin, begin + size);
    }, &encoded, width, height, 4, pixels.data(), width * 4);
    // clang-format on
    return encoded;
}

} // namespace haylen::test
