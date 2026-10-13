#include <gtest/gtest.h>
#include <lua.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "haylen/core/Log.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Error.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/NativeProperty.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "lua/WeakReference.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::lua {

// A bound type of the tests with fields of the kinds that `ClassBuilder` records as native properties and of kinds it does not, an accessor pair and integer keys.
struct Probe {
    struct Order {
        float depth = 0.0F;
        int layer = 0;
    };

    [[nodiscard]] float getScale() const noexcept {
        return scale;
    }
    void setScale(float value) noexcept {
        scale = value;
    }

    float speed = 0.0F;
    math::Vec2 position;
    math::Color tint;
    Order order;
    bool visible = true;
    float scale = 1.0F;
    std::array<float, 2> values{};
};

template <> struct Type<Probe> {
    static constexpr const char* name = "haylen.test.Probe";
    using Storage = std::shared_ptr<Probe>;
};

// Binds `Probe` with the toolkit the bindings of the engine use, together with a function that takes the shared object.
class ProbeLua final {
  public:
    static void install(lua_State* L) {
        ClassBuilder<Probe>(L).field<&Probe::speed>("speed").field<&Probe::position>("position").field<&Probe::tint>("tint").field<&Probe::visible>("visible").nestedField<&Probe::order, &Probe::Order::depth>("depth").nestedField<&Probe::order, &Probe::Order::layer>("layer").accessor<&Probe::getScale, &Probe::setScale>("scale").indexer(&at, &assign).install();
        lua_register(L, "probeSpeed", &Binding::function<&speedOf>);
    }

  private:
    static float speedOf(const std::shared_ptr<Probe>& probe) {
        return probe->speed;
    }

    static std::size_t checkIndex(lua_State* L) {
        const lua_Integer index = luaL_checkinteger(L, 2);
        luaL_argcheck(L, index >= 1 && index <= 2, 2, "index out of range");
        return static_cast<std::size_t>(index - 1);
    }

    static int at(lua_State* L) {
        lua_pushnumber(L, static_cast<lua_Number>(Userdata::check<Probe>(L, 1).values[checkIndex(L)]));
        return 1;
    }

    static int assign(lua_State* L) {
        Userdata::check<Probe>(L, 1).values[checkIndex(L)] = static_cast<float>(luaL_checknumber(L, 3));
        return 0;
    }
};

TEST(EnvironmentTest, RequiresModulesFromThePackageOnly) {
    // clang-format off
    test::EngineFixture fixture({
        {"source/main.lua", "loadedFromMain = require('app.config').name"},
        {"source/app/config.lua", "return {name = 'config'}"},
        {"source/app/world/init.lua", "return {name = 'world'}"},
        {"source/app/broken.lua", "return {"},
        {"content/app/outside.lua", "return {name = 'outside'}"},
    });
    // clang-format on

    EXPECT_EQ(fixture.lua("return loadedFromMain"), "config");
    EXPECT_EQ(fixture.lua("return require('app.world').name"), "world");
    EXPECT_EQ(fixture.lua("return require('app.config') == require('app.config')"), "true");
    EXPECT_EQ(fixture.lua("return package.path .. package.cpath"), "");
    EXPECT_NE(fixture.lua("return require('app.missing')").find("no file 'source/app/missing.lua' or 'source/app/missing/init.lua' in the app package"), std::string::npos);
    EXPECT_NE(fixture.lua("return require('app.broken')").find("source/app/broken.lua"), std::string::npos);
    EXPECT_NE(fixture.lua("return require('app.outside')").find("no file 'source/app/outside.lua'"), std::string::npos);
    EXPECT_NE(fixture.lua("return require('os.missing')").find("error: "), std::string::npos);
}

TEST(EnvironmentTest, RequiresTheModulesOfThePluginsOfTheApp) {
    // clang-format off
    test::EngineFixture fixture({
        {"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "plugins": {"ads-kit": {}}})"},
        {"plugins/ads-kit/plugin.json", R"({"id": "ads-kit", "version": "1.0.0"})"},
        {"plugins/ads-kit/source/init.lua", "return {name = 'ads', banner = require('ads-kit.banner').name}"},
        {"plugins/ads-kit/source/banner.lua", "return {name = 'banner'}"},
        {"plugins/ads-kit/source/formats/init.lua", "return {name = 'formats'}"},
        {"plugins/ads-kit/source/broken.lua", "return {"},
        {"plugins/other/plugin.json", R"({"id": "other", "version": "1.0.0"})"},
        {"plugins/other/source/init.lua", "return {}"},
        {"source/main.lua", "ads = require('ads-kit')"},
    });
    // clang-format on

    EXPECT_EQ(fixture.lua("return ads.name .. ' ' .. ads.banner"), "ads banner");
    EXPECT_EQ(fixture.lua("return select(2, require('ads-kit.formats'))"), "plugins/ads-kit/source/formats/init.lua");
    EXPECT_EQ(fixture.lua("return require('ads-kit.formats').name"), "formats");
    EXPECT_NE(fixture.lua("return require('ads-kit.broken')").find("plugins/ads-kit/source/broken.lua"), std::string::npos);
    EXPECT_NE(fixture.lua("return require('ads-kit.missing')").find("no file 'plugins/ads-kit/source/missing.lua' or 'plugins/ads-kit/source/missing/init.lua' in the app package"), std::string::npos);

    // Only the plugins that `app.json` lists have modules, so the folder of any other plugin is invisible.
    EXPECT_NE(fixture.lua("return require('other')").find("no file 'source/other.lua' or 'source/other/init.lua' in the app package"), std::string::npos);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(EnvironmentTest, StopsAnAppWhoseModuleHasTheNameOfAPlugin) {
    {
        // clang-format off
        test::EngineFixture fixture({
            {"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "plugins": {"ads-kit": {}}})"},
            {"plugins/ads-kit/plugin.json", R"({"id": "ads-kit", "version": "1.0.0"})"},
            {"source/ads-kit/about.txt", "Not a module."},
            {"source/ads-kit/banner.lua", "return {}"},
            {"source/main.lua", "started = true"},
        });
        // clang-format on

        ASSERT_NE(fixture.engine().getError(), nullptr);
        EXPECT_STREQ(fixture.engine().getError()->what(), "The app module \"source/ads-kit/banner.lua\" has the name \"ads-kit.banner\", which \"require\" resolves to \"plugins/ads-kit/source/banner.lua\" of the plugin \"ads-kit\". Rename the module of the app.");
        EXPECT_EQ(fixture.lua("return started"), "nil");
    }

    test::EngineFixture shadowed({{"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "plugins": {"ads-kit": {}}})"}, {"plugins/ads-kit/plugin.json", "{}"}, {"source/ads-kit/init.lua", "return {}"}});
    ASSERT_NE(shadowed.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(shadowed.engine().getError()->what()).find("source/ads-kit/init.lua\" has the name \"ads-kit\", which \"require\" resolves to \"plugins/ads-kit/source/init.lua\""), std::string::npos);
}

TEST(EnvironmentTest, ReportsErrorsWithTheirStack) {
    test::EngineFixture fixture;
    lua_State* L = fixture.lua();

    fixture.runLua("function explode() error('boom') end");
    lua_getglobal(L, "explode");
    try {
        Runtime::protectedCall(L, 0, 0);
        FAIL() << "The call should fail.";
    } catch (const Error& error) {
        EXPECT_STREQ(error.what(), "test:1: boom");
        ASSERT_EQ(error.getFrames().size(), 2U);
        EXPECT_EQ(error.getFrames()[1].getLocation(), "test:1");
        EXPECT_EQ(error.getFrames()[1].function, "function <test:1>");
    }
    EXPECT_EQ(lua_gettop(L), 0);

    EXPECT_THROW(Runtime::runChunk(L, "local = 1", "=broken"), Error);
    EXPECT_THROW(Runtime::runChunk(L, "\x1bLua", "=binary"), Error);
    EXPECT_THROW(Runtime::runChunk(L, "error('at run time')", "=failing"), Error);
    Runtime::runChunk(L, "ranChunk = 'yes'", "=chunk");
    EXPECT_EQ(fixture.lua("return ranChunk"), "yes");
    EXPECT_EQ(lua_gettop(L), 0);

    Runtime::runReporting(L, [] { throw std::runtime_error("reported"); });
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "reported");
    EXPECT_TRUE(fixture.engine().getError()->getFrames().empty());
}

TEST(WarningsTest, WritesTheWarningsOfLuaToTheLog) {
    test::EngineFixture fixture;
    std::vector<std::string> lines;
    // clang-format off
    const std::uint64_t listener = core::Log::addListener([&lines](core::Log::Level level, std::string_view line) {
        if (level == core::Log::Level::Warning) {
            lines.emplace_back(line);
        }
    });
    fixture.runLua(R"(
        setmetatable({}, {__gc = function() error('The finalizer failed.') end})
        collectgarbage()
        collectgarbage()
        warn('@off')
        warn('Hidden.')
        warn('@on')
        warn('Shown ', 'in pieces.')
    )");
    // clang-format on
    core::Log::removeListener(listener);

    ASSERT_EQ(lines.size(), 2U);
    EXPECT_NE(lines[0].find("Lua warning: error in __gc (test:2: The finalizer failed.)"), std::string::npos) << lines[0];
    EXPECT_NE(lines[1].find("Lua warning: Shown in pieces."), std::string::npos) << lines[1];
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(EnvironmentTest, NamesTheOptionThatHoldsABadValue) {
    test::EngineFixture fixture;
    fixture.runLua("graphics2d = require('haylen.graphics2d') tween = require('haylen.tween') box = {x = 0}");

    EXPECT_NE(fixture.lua("graphics2d.beginScreen({sort = 'random'})").find("The option \"sort\" of \"beginScreen\" is invalid: unknown value 'random'."), std::string::npos);
    EXPECT_NE(fixture.lua("tween.to(box, 1, {x = 1}, {delay = 'soon'})").find("The option \"delay\" of \"to\" is invalid: number expected, got string."), std::string::npos);
    EXPECT_EQ(fixture.lua("tween.to(box, 1, {x = 1}, {delay = 0.5}) return 'ok'"), "ok");
}

TEST(EnvironmentTest, LoadsChunksOnlyAsText) {
    test::EngineFixture fixture;
    lua_State* L = fixture.lua();
    const test::TemporaryDirectory folder;
    folder.write("text.lua", "return 7");
    folder.write("binary.lua", "\x1bLua");
    lua_pushstring(L, folder.getPath().generic_string().c_str());
    lua_setglobal(L, "folder");

    // Real bytecode comes from the C API, since app code has no `string.dump`.
    std::string bytecode;
    ASSERT_EQ(luaL_loadstring(L, "return 42"), LUA_OK);
    // clang-format off
    lua_dump(L, [](lua_State*, const void* data, std::size_t size, void* output) {
        static_cast<std::string*>(output)->append(static_cast<const char*>(data), size);
        return 0;
    }, &bytecode, 0);
    // clang-format on
    lua_pop(L, 1);
    lua_pushlstring(L, bytecode.data(), bytecode.size());
    lua_setglobal(L, "bytecode");

    EXPECT_EQ(fixture.lua("return load('return 1 + 1')() + load('return 3', 'three', 't')()"), "5");
    EXPECT_EQ(fixture.lua("local parts = {'return ', '4'} return load(function() return table.remove(parts, 1) end)()"), "4");
    EXPECT_EQ(fixture.lua("local env = {} load('value = 5', 'chunk', nil, env)() return env.value .. ' ' .. tostring(value)"), "5 nil");
    EXPECT_EQ(fixture.lua("return select(2, load(bytecode))"), "attempt to load a binary chunk (mode is 't')");
    EXPECT_NE(fixture.lua("return load(bytecode, 'bytes', 'b')").find("bad argument #3 to 'load' (chunks load only as text, so the mode is 't')"), std::string::npos);
    EXPECT_EQ(fixture.lua("return type(string.dump) .. ' ' .. type(('').dump)"), "nil nil");

    EXPECT_EQ(fixture.lua("return loadfile(folder .. '/text.lua')() + dofile(folder .. '/text.lua')"), "14");
    EXPECT_NE(fixture.lua("return select(2, loadfile(folder .. '/binary.lua'))").find("attempt to load a binary chunk (mode is 't')"), std::string::npos);
    EXPECT_NE(fixture.lua("return loadfile(folder .. '/text.lua', 'bt')").find("bad argument #2 to 'loadfile'"), std::string::npos);
    EXPECT_NE(fixture.lua("return dofile(folder .. '/binary.lua')").find("attempt to load a binary chunk (mode is 't')"), std::string::npos);
}

TEST(EnvironmentTest, KeepsEngineMetatablesOutOfReach) {
    test::EngineFixture fixture;
    fixture.runLua("point = require('haylen.math').vec2(1, 2)");

    EXPECT_EQ(fixture.lua("return getmetatable(point) .. ' ' .. debug.getmetatable(point)"), "haylen.Vec2 haylen.Vec2");
    EXPECT_NE(fixture.lua("setmetatable({}, getmetatable(point))").find("bad argument #2 to 'setmetatable'"), std::string::npos);
    EXPECT_NE(fixture.lua("debug.setmetatable(point, nil)").find("The metatable of this value is protected and cannot be changed."), std::string::npos);
    EXPECT_EQ(fixture.lua("return point.x + point:length() * 0"), "1.0");
    EXPECT_EQ(fixture.lua("return type(debug.getregistry)"), "nil");

    // Lua values keep their ordinary metatables and upvalues, while native functions show none.
    EXPECT_EQ(fixture.lua("local t = debug.setmetatable({}, {__index = {answer = 42}}) return t.answer"), "42");
    EXPECT_EQ(fixture.lua("local hidden = 3 local function f() return hidden end return select('#', debug.getupvalue(load, 1)) .. ' ' .. select('#', debug.setupvalue(load, 1, print)) .. ' ' .. debug.getupvalue(f, 1) .. ' ' .. debug.setupvalue(f, 1, 4) .. ' ' .. f()"), "0 0 hidden hidden 4");
    EXPECT_EQ(fixture.lua("return load('return 6')()"), "6");
}

// The stack slots and the functions of native frames hold the state of the engine while they run, and the user values of userdata hold what engine values keep, which scripts could otherwise overwrite.
TEST(EnvironmentTest, KeepsNativeFramesAndUserValuesOutOfReach) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        local log = {}
        table.sort({3, 1, 2}, function(a, b)
            if #log == 0 then
                log[1] = tostring(debug.getlocal(2, 1))
                log[2] = select(2, pcall(debug.setlocal, 3, 1, 0))
                log[3] = tostring(debug.getinfo(2, 'f').func)
                log[4] = debug.getinfo(2, 'S').what
            end
            return a < b
        end)
        report = table.concat(log, ' | ')
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return report"), "nil | The locals of native functions cannot be changed. | nil | C");
    EXPECT_EQ(fixture.lua("return type(debug.getuservalue) .. ' ' .. type(debug.setuservalue)"), "nil nil");

    // Levels still count from the script that calls the debug library, which reads and changes the locals of Lua functions as before.
    EXPECT_EQ(fixture.lua("local first = 5 local name, value = debug.getlocal(1, 1) return name .. ' ' .. value"), "first 5");
    EXPECT_EQ(fixture.lua("local first = 1 debug.setlocal(1, 1, 9) return first"), "9");
    EXPECT_EQ(fixture.lua("local function where() return debug.getinfo(2, 'S').what .. ' ' .. debug.getinfo(1, 'n').name end local result = where() return result"), "main where");
    EXPECT_EQ(fixture.lua("return debug.getinfo(1, 'f').func ~= nil"), "true");

    // A function given in place of a level is no frame, so its parameters and its information stay readable.
    EXPECT_EQ(fixture.lua("return debug.getlocal(function(first, second) end, 2)"), "second");
    EXPECT_EQ(fixture.lua("local info = debug.getinfo(print) return info.what .. ' ' .. tostring(info.func == print)"), "C true");
}

TEST(PromiseTest, SettlesFromNativeCodeOnAnyThread) {
    test::EngineFixture fixture;
    lua_State* L = fixture.lua();
    const Promise loaded(fixture.engine());
    const Promise failed(fixture.engine());
    const Promise custom(fixture.engine());
    loaded.push(L);
    lua_setglobal(L, "loaded");
    failed.push(L);
    lua_setglobal(L, "failed");
    custom.push(L);
    lua_setglobal(L, "custom");

    // clang-format off
    fixture.runLua(R"(
        results = {}
        require('async').spawn(function()
            local map = loaded:await()
            local value, err = failed:await()
            results = {map.name, map.size[2], tostring(value), err, custom:await()}
        end)
    )");
    // clang-format on
    EXPECT_FALSE(loaded.isSettled());

    std::thread worker([&] { loaded.resolve(core::Json{{"name", "island"}, {"size", {56, 36}}}); });
    worker.join();
    failed.reject("The map is missing.");
    custom.resolveWith([](lua_State* state) { lua_pushinteger(state, 7); });
    EXPECT_TRUE(loaded.isSettled());
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #results") == "5"; }));
    EXPECT_EQ(fixture.lua("return table.concat(results, ' ')"), "island 36 nil The map is missing. 7");
}

// Varn copies the function that pushes the value every time a coroutine awaits, so a large reply must never be copied with it.
TEST(PromiseTest, SharesTheResolvedValueWithEveryAwait) {
    test::EngineFixture fixture;
    lua_State* L = fixture.lua();
    const Promise reply(fixture.engine());
    reply.push(L);
    lua_setglobal(L, "reply");
    // clang-format off
    fixture.runLua(R"(
        sizes = {}
        for task = 1, 8 do
            require('async').spawn(function()
                local size = #reply:await()
                sizes[#sizes + 1] = size
            end)
        end
    )");
    // clang-format on
    reply.resolve(core::Json(std::vector<int>(100000, 7)));

    const test::AllocationTracker tracker;
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #sizes") == "8"; }));
    EXPECT_EQ(fixture.lua("return sizes[8]"), "100000");
    EXPECT_LT(tracker.getTotal(), 1000000U) << "The eight awaits copied the reply.";
}

TEST(PromiseTest, RejectsValuesThatLuaCannotHold) {
    test::EngineFixture fixture;
    lua_State* L = fixture.lua();
    const Promise deep(fixture.engine());
    const Promise binary(fixture.engine());
    const Promise shallow(fixture.engine());
    deep.push(L);
    lua_setglobal(L, "deep");
    binary.push(L);
    lua_setglobal(L, "binary");
    shallow.push(L);
    lua_setglobal(L, "shallow");

    // clang-format off
    fixture.runLua(R"(
        results = {}
        require('async').spawn(function()
            for _, promise in ipairs({deep, binary, shallow}) do
                local value, err = promise:await()
                results[#results + 1] = err or type(value)
            end
        end)
    )");
    // clang-format on
    deep.resolve(core::Json::parse(std::string(200, '[') + std::string(200, ']')));
    binary.resolve(core::Json{{"payload", core::Json::binary({1, 2})}});
    shallow.resolve(core::Json::parse(std::string(100, '[') + std::string(100, ']')));
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #results") == "3"; }));
    EXPECT_EQ(fixture.lua("return table.concat(results, ' | ')"), "JSON value is nested too deeply to push to Lua. | table | table");
}

TEST(ErrorTest, CapturesTheStackOfSceneCallbacks) {
    test::EngineFixture fixture({{"source/scenes/battle.lua", "local function strike(enemy)\n  return enemy.health\nend\nreturn function()\n  local damage = strike(nil)\n  return damage\nend"}});
    fixture.runLua("require('haylen.scene').push({update = require('scenes.battle')})");
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);

    const Error& error = *fixture.engine().getError();
    EXPECT_EQ(error.getFile(), "source/scenes/battle.lua");
    EXPECT_EQ(error.getLine(), 2);
    EXPECT_TRUE(error.getMessage().starts_with("attempt to index")) << error.getMessage();

    // The scene function is the last frame, because the protected call that runs it belongs to the engine.
    ASSERT_EQ(error.getFrames().size(), 2U);
    EXPECT_EQ(error.getFrames()[0].getLocation(), "source/scenes/battle.lua:2");
    EXPECT_EQ(error.getFrames()[0].function, "upvalue 'strike'");
    EXPECT_EQ(error.getFrames()[1].getLocation(), "source/scenes/battle.lua:5");
    EXPECT_EQ(error.getFrames()[1].function, "function <source/scenes/battle.lua:4>");
    EXPECT_EQ(error.getFrames()[1].kind, Error::Frame::Kind::Lua);

    EXPECT_EQ(error.getTraceback(), "source/scenes/battle.lua:2  upvalue 'strike'\nsource/scenes/battle.lua:5  function <source/scenes/battle.lua:4>");
    EXPECT_EQ(error.getTraceback().find('\t'), std::string::npos);
    const core::Json json = error.toJson();
    EXPECT_EQ(json.at("line"), 2);
    EXPECT_EQ(json.at("traceback"), error.getTraceback());
    ASSERT_EQ(json.at("frames").size(), 2U);
    EXPECT_EQ(json.at("frames")[1], (core::Json{{"source", "source/scenes/battle.lua"}, {"line", 5}, {"function", "function <source/scenes/battle.lua:4>"}, {"kind", "lua"}}));
}

TEST(ErrorTest, KeepsTheEndsOfARunawayRecursion) {
    test::EngineFixture fixture;
    fixture.runLua("local function dive(depth) return dive(depth + 1) + 1 end require('haylen.scene').push({update = function() dive(1) end})");
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);

    const Error& error = *fixture.engine().getError();
    EXPECT_NE(error.getMessage().find("stack overflow"), std::string::npos) << error.getMessage();
    EXPECT_LE(error.getFrames().size(), 22U);
    EXPECT_TRUE(std::ranges::any_of(error.getFrames(), [](const Error::Frame& frame) { return frame.function.ends_with("levels skipped"); }));
}

// Failures of tasks reach the engine with the frames of where they were raised, which Varn lists from the innermost call out.
TEST(ErrorTest, ReadsTheStackOfTasksFromTheirFrames) {
    {
        // The frames name functions like the frames of the protected calls, and the native entry of Varn that runs the task is left out.
        test::EngineFixture fixture;
        fixture.runLua("local function dive(depth) if depth == 0 then error('too deep') end dive(depth - 1) end require('async').spawn(function() dive(2) end)");
        ASSERT_TRUE(fixture.frameUntil([&] { return fixture.engine().getError() != nullptr; }));
        const std::vector<Error::Frame>& frames = fixture.engine().getError()->getFrames();
        ASSERT_EQ(frames.size(), 5U);
        EXPECT_EQ(frames[0].getLocation(), "[C]");
        EXPECT_EQ(frames[0].function, "global 'error'");
        EXPECT_EQ(frames[0].kind, Error::Frame::Kind::C);
        EXPECT_EQ(frames[1].getLocation(), "test:1");
        EXPECT_EQ(frames[1].function, "upvalue 'dive'");
        EXPECT_EQ(frames[1].kind, Error::Frame::Kind::Lua);
        EXPECT_EQ(frames.back().getLocation(), "test:1");
        EXPECT_EQ(frames.back().function, "function <test:1>");
    }
    {
        // A stack deeper than Varn lists keeps its innermost and its outermost frames around the line the protected calls show for the levels they skip.
        test::EngineFixture fixture;
        fixture.runLua("local function dive(depth) if depth == 0 then error('too deep') end dive(depth - 1) end require('async').spawn(function() dive(200) end)");
        ASSERT_TRUE(fixture.frameUntil([&] { return fixture.engine().getError() != nullptr; }));
        const std::vector<Error::Frame>& frames = fixture.engine().getError()->getFrames();
        ASSERT_EQ(frames.size(), 64U);
        EXPECT_EQ(frames.front().function, "global 'error'");
        EXPECT_EQ(frames[31].function, "upvalue 'dive'");
        EXPECT_EQ(frames[32].getLocation(), "...");
        EXPECT_EQ(frames[32].function, "140 levels skipped");
        EXPECT_EQ(frames[32].kind, Error::Frame::Kind::C);
        EXPECT_EQ(frames[33].function, "upvalue 'dive'");
        EXPECT_EQ(frames.back().function, "function <test:1>");
    }

    test::EngineFixture fixture;
    fixture.runLua("require('async').spawn(load('error(\"chunk failure\")', '=chunk'))");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.engine().getError() != nullptr; }));
    EXPECT_EQ(fixture.engine().getError()->getFrames().back().kind, Error::Frame::Kind::Main);
    EXPECT_EQ(fixture.engine().getError()->getFrames().back().function, "main chunk");
    EXPECT_EQ(fixture.engine().getError()->getFrames().back().getLocation(), "chunk:1");
}

TEST(ErrorTest, FindsTheScriptPositionOfErrors) {
    const Error nested("The scene could not start: source/main.lua:12: missing sprite");
    EXPECT_EQ(nested.getMessage(), "The scene could not start: source/main.lua:12: missing sprite");
    EXPECT_EQ(nested.getFile(), "source/main.lua");
    EXPECT_EQ(nested.getLine(), 12);

    const Error plain("The app could not be loaded.");
    EXPECT_EQ(plain.getMessage(), "The app could not be loaded.");
    EXPECT_TRUE(plain.getFile().empty());
    EXPECT_EQ(plain.getLine(), 0);
    EXPECT_TRUE(plain.getTraceback().empty());

    const Error huge("source/main.lua:99999999999: a line number beyond any int");
    EXPECT_TRUE(huge.getFile().empty());
    EXPECT_EQ(huge.getLine(), 0);

    // A position further in takes the whole chunk name, and a long message without spaces is read in one pass.
    const Error later("x.lua:1 then deep/scene.lua.lua:3: failed");
    EXPECT_EQ(later.getFile(), "deep/scene.lua.lua");
    EXPECT_EQ(later.getLine(), 3);
    EXPECT_EQ(later.getMessage(), "x.lua:1 then deep/scene.lua.lua:3: failed");
    const Error blob(std::string(1000000, 'a') + ".lua:5: end");
    EXPECT_EQ(blob.getLine(), 5);
    EXPECT_EQ(blob.getMessage(), "end");

    // A message without a position points at the innermost script frame, skipping native ones.
    const Error value("(error object is a table value)", {{.source = "[C]", .function = "global 'error'", .kind = Error::Frame::Kind::C}, {.source = "source/main.lua", .line = 7, .function = "main chunk", .kind = Error::Frame::Kind::Main}});
    EXPECT_EQ(value.getFile(), "source/main.lua");
    EXPECT_EQ(value.getLine(), 7);
    EXPECT_EQ(value.getTraceback(), "[C]                global 'error'\nsource/main.lua:7  main chunk");
}

// An error value that is no text and describes itself as no text names its type, like the stand-alone interpreter.
TEST(ErrorTest, NamesTheTypeOfErrorValuesWithoutText) {
    test::EngineFixture fixture;
    lua_State* L = fixture.lua();
    fixture.runLua("function raiseTable() error({}) end function raiseBlank() error(setmetatable({}, {__tostring = function() return '' end})) end");
    for (const char* function : {"raiseTable", "raiseBlank"}) {
        lua_getglobal(L, function);
        try {
            Runtime::protectedCall(L, 0, 0);
            ADD_FAILURE() << "The call of \"" << function << "\" should fail.";
        } catch (const Error& error) {
            EXPECT_STREQ(error.what(), "(error object is a table value)") << function;
        }
    }
}

// The to-be-closed variables of a failed hook receive the error that the engine reports, which reads as its message.
TEST(ErrorTest, HandsTheErrorToTheClosingVariablesOfAFailedHook) {
    test::EngineFixture fixture;
    fixture.runLua("require('haylen.scene').push({update = function() local guard <close> = setmetatable({}, {__close = function(_, err) closedWith = tostring(err) end}) error('the update broke') end})");
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_EQ(fixture.lua("return closedWith"), "test:1: the update broke");
}

TEST(EnvironmentTest, RejectsStatesWithoutAnEngine) {
    lua_State* L = luaL_newstate();
    // clang-format off
    lua_pushcfunction(L, [](lua_State* state) -> int {
        (void)Runtime::getEngine(state);
        return 0;
    });
    // clang-format on
    ASSERT_NE(lua_pcall(L, 0, 0, 0), LUA_OK);
    EXPECT_NE(std::string(lua_tostring(L, -1)).find("not bound to a Haylen engine"), std::string::npos);
    lua_close(L);
}

TEST(ReferenceTest, KeepsValuesAliveAndReleasesThem) {
    test::EngineFixture fixture;
    lua_State* L = fixture.lua();

    EXPECT_FALSE(Reference().isValid());
    lua_pushstring(L, "kept");
    Reference reference(L, -1);
    lua_pop(L, 1);
    EXPECT_TRUE(reference.isValid());
    reference.push(L);
    EXPECT_STREQ(lua_tostring(L, -1), "kept");
    lua_pop(L, 1);

    Reference moved(std::move(reference));
    EXPECT_EQ(moved.getState(), L);

    lua_State* thread = lua_newthread(L);
    lua_pushstring(thread, "from a thread");
    const Reference threaded(thread, -1);
    EXPECT_EQ(threaded.getState(), L);
    EXPECT_EQ(Runtime::getMainThread(thread), L);
    lua_settop(L, 0);
    Reference assigned;
    assigned = std::move(moved);
    assigned.push(L);
    EXPECT_STREQ(lua_tostring(L, -1), "kept");
    lua_pop(L, 1);
    EXPECT_TRUE(assigned.isValid());
    assigned.reset();
    assigned.reset();
    EXPECT_FALSE(assigned.isValid());
}

TEST(JsonConverterTest, ConvertsBetweenTablesAndJson) {
    test::EngineFixture fixture;
    lua_State* L = fixture.lua();

    const core::Json document = core::Json::parse(R"({"name": "island", "days": 3, "ratio": 0.5, "alive": true, "tags": ["a", "b"], "empty": [], "nested": {"deep": {"value": null}}})");
    JsonConverter::push(L, document);
    lua_setglobal(L, "document");
    EXPECT_EQ(fixture.lua("return document.name .. document.days .. document.ratio .. tostring(document.alive)"), "island30.5true");
    EXPECT_EQ(fixture.lua("return #document.tags .. document.tags[2]"), "2b");
    EXPECT_EQ(fixture.lua("return document.nested.deep.value"), "nil");

    // Unsigned integers beyond the Lua integers stay positive, and keys keep every byte.
    JsonConverter::push(L, core::Json::parse(R"({"huge": 18446744073709551615, "a\u0000b": 1})"));
    lua_setglobal(L, "wide");
    EXPECT_EQ(fixture.lua("return tostring(wide.huge > 0) .. ' ' .. tostring(wide['a\\0b']) .. ' ' .. tostring(wide.a)"), "true 1 nil");

    fixture.runLua("roundTrip ={list = {1, 2, 3}, map = {x = 1.5}, flag = false, text = 'hi', integer = 7, float = 2.0}");
    lua_getglobal(L, "roundTrip");
    const core::Json converted = JsonConverter::read(L, -1);
    lua_pop(L, 1);
    EXPECT_EQ(converted.at("list"), core::Json::parse("[1, 2, 3]"));
    EXPECT_EQ(converted.at("map").at("x"), 1.5);
    EXPECT_EQ(converted.at("flag"), false);
    EXPECT_EQ(converted.at("text"), "hi");
    EXPECT_TRUE(converted.at("integer").is_number_integer());
    EXPECT_TRUE(converted.at("float").is_number_float());

    fixture.runLua("emptyTable = {}");
    lua_getglobal(L, "emptyTable");
    EXPECT_TRUE(JsonConverter::read(L, -1).is_object());
    lua_pop(L, 1);

    fixture.runLua("sparse = {[1] = 'a', [3] = 'c'}");
    lua_getglobal(L, "sparse");
    EXPECT_TRUE(JsonConverter::read(L, -1).is_object());
    lua_pop(L, 1);

    fixture.runLua("mixed = {1, 2, name = 'three'}");
    lua_getglobal(L, "mixed");
    EXPECT_EQ(JsonConverter::read(L, -1), core::Json::parse(R"({"1": 1, "2": 2, "name": "three"})"));
    lua_pop(L, 1);

    fixture.runLua("counted = setmetatable({}, {__len = function() error('never called') end})");
    lua_getglobal(L, "counted");
    EXPECT_TRUE(JsonConverter::read(L, -1).is_object());
    lua_settop(L, 0);

    for (const char* invalid : {"return (function() local cyclic = {} cyclic.self = cyclic return cyclic end)()", "return {callback = print}", "return {[true] = 1}"}) {
        ASSERT_EQ(luaL_dostring(L, invalid), LUA_OK);
        EXPECT_THROW((void)JsonConverter::read(L, -1), std::invalid_argument) << invalid;
        lua_settop(L, 0);
    }
    JsonConverter::push(L, core::Json::binary({1, 2}));
    std::size_t length = 0;
    const char* text = lua_tolstring(L, -1, &length);
    EXPECT_EQ(std::string_view(text, length), std::string_view("\x01\x02", 2));
    lua_settop(L, 0);

    const auto nested = [](int levels) { return core::Json::parse(std::string(static_cast<std::size_t>(levels), '[') + std::string(static_cast<std::size_t>(levels), ']')); };
    JsonConverter::push(L, nested(100));
    lua_settop(L, 0);
    EXPECT_THROW(JsonConverter::push(L, nested(100000)), std::invalid_argument);
    lua_settop(L, 0);
}

TEST(BindingTest, ChecksBoundTypesAndMembers) {
    test::EngineFixture fixture;
    fixture.runLua("math2 = require('haylen.math') point = math2.vec2(1, 2)");

    EXPECT_EQ(fixture.lua("point.x = 5 return point.x"), "5.0");
    EXPECT_NE(fixture.lua("return point.missing").find("Vec2\" has no member \"missing\""), std::string::npos);
    EXPECT_NE(fixture.lua("point.length = 1").find("Vec2\" has no writable property \"length\""), std::string::npos);
    EXPECT_NE(fixture.lua("point.x = 'text'").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("return point.length(42)").find("error: "), std::string::npos);
    EXPECT_EQ(fixture.lua("return point:length() > 0"), "true");
}

// A reader takes the keys of its own list in one pass, leaves the keys of the other readers of the table alone and rejects any other key.
TEST(TableTest, ReadsTheKeysAnOptionTableHoldsInOnePass) {
    test::EngineFixture fixture;
    lua_State* L = fixture.lua();
    // clang-format off
    constexpr lua_CFunction read = [](lua_State* state) -> int {
        static constexpr std::array<std::string_view, 3> kOwn{"speed", "name", "visible"};
        static constexpr std::array<std::string_view, 1> kOther{"layer"};
        float speed = 1.0F;
        std::string name = "none";
        bool visible = true;
        Table::readFields(state, 1, kOwn, {kOther}, [state, &speed, &name, &visible](std::string_view key) {
            if (key == "speed") {
                Table::readValue(state, key, speed);
            } else if (key == "name") {
                Table::readValue(state, key, name);
            } else if (key == "visible") {
                Table::readValue(state, key, visible);
            }
        });
        lua_pushfstring(state, "%f %s %s", static_cast<double>(speed), name.c_str(), visible ? "true" : "false");
        return 1;
    };
    // clang-format on
    lua_register(L, "readOptions", read);

    EXPECT_EQ(fixture.lua("return readOptions({speed = 2.5, visible = false, layer = 3})"), "2.5 none false");
    EXPECT_EQ(fixture.lua("return readOptions({name = 'fast', speed = '4'})"), "4.0 fast true");
    EXPECT_NE(fixture.lua("return readOptions({sped = 2})").find("Unknown option \"sped\"."), std::string::npos);
    EXPECT_NE(fixture.lua("return readOptions({2})").find("Option tables only accept string keys."), std::string::npos);
    EXPECT_NE(fixture.lua("return readOptions({speed = 'fast'})").find("The option \"speed\" of \"readOptions\" is invalid: number expected, got string."), std::string::npos);
}

// A reader that checks the keys first rejects every key it does not name, and an option whose converter throws names the option with the reason.
TEST(TableTest, ChecksTheKeysAndValuesOfAnOptionTable) {
    test::EngineFixture fixture;
    lua_State* L = fixture.lua();
    // clang-format off
    constexpr lua_CFunction read = [](lua_State* state) -> int {
        static constexpr std::array<std::string_view, 2> kFields{"name", "data"};
        Table::checkFields(state, 1, {kFields});
        std::string name = "none";
        core::Json data;
        Table::readField(state, 1, "name", name);
        Table::readField(state, 1, "data", data);
        lua_pushfstring(state, "%s %s", name.c_str(), data.dump().c_str());
        return 1;
    };
    // clang-format on
    lua_register(L, "checkOptions", read);

    EXPECT_EQ(fixture.lua("return checkOptions({data = {1, 2}})"), "none [1,2]");
    EXPECT_NE(fixture.lua("return checkOptions({names = 'x'})").find("Unknown option \"names\"."), std::string::npos);
    EXPECT_NE(fixture.lua("return checkOptions({'x'})").find("Option tables only accept string keys."), std::string::npos);
    EXPECT_NE(fixture.lua("return checkOptions({data = {callback = print}})").find("The option \"data\" of \"checkOptions\" is invalid: A function cannot be converted to JSON."), std::string::npos);
}

TEST(BindingTest, RefusesAModuleNameThatIsTaken) {
    test::EngineFixture fixture;
    lua_State* L = fixture.lua();
    // clang-format off
    constexpr lua_CFunction open = [](lua_State* state) -> int {
        lua_pushliteral(state, "fresh");
        return 1;
    };
    // clang-format on

    Binding::preload(L, "app.fresh", open);
    EXPECT_EQ(fixture.lua("return require('app.fresh')"), "fresh");
    for (const char* taken : {"app.fresh", "haylen.graphics", "json"}) {
        try {
            Binding::preload(L, taken, open);
            ADD_FAILURE() << "The module \"" << taken << "\" should be refused.";
        } catch (const std::runtime_error& error) {
            EXPECT_EQ(std::string(error.what()), "The Lua module \"" + std::string(taken) + "\" cannot be added, because a module of Varn or of another plugin already has its name. Give the module another name.");
        }
    }
    EXPECT_EQ(fixture.lua("return type(require('json').encode)"), "function");
}

// Fields, nested fields and accessors of numbers, `Vec2` and `Color` values are native properties, which tweens animate in C++ and which reach nothing once the object is released.
TEST(ClassBuilderTest, RecordsTheNativePropertiesThatTweensAnimate) {
    test::EngineFixture fixture;
    lua_State* L = fixture.lua();
    ProbeLua::install(L);
    const auto probe = std::make_shared<Probe>();
    Stack::push(L, probe);
    lua_setglobal(L, "probe");

    EXPECT_EQ(fixture.lua("probe.speed = 2 probe.depth = 3 probe.layer = 4 probe.scale = 5 probe[2] = 6 return probe.speed + probe.depth + probe.layer + probe.scale + probe[2] + probeSpeed(probe)"), "22.0");
    EXPECT_EQ(probe->order.layer, 4);
    EXPECT_EQ(probe->values[1], 6.0F);
    EXPECT_NE(fixture.lua("probe.missing = 1").find("The type \"haylen.test.Probe\" has no writable property \"missing\"."), std::string::npos);
    EXPECT_NE(fixture.lua("probe[3] = 1").find("index out of range"), std::string::npos);

    fixture.runLua("require('haylen.tween').to(probe, 0.25, {speed = 8, depth = 10, scale = 2})");
    fixture.frames(30);
    EXPECT_FLOAT_EQ(probe->speed, 8.0F);
    EXPECT_FLOAT_EQ(probe->order.depth, 10.0F);
    EXPECT_FLOAT_EQ(probe->scale, 2.0F);

    lua_getglobal(L, "probe");
    const int userdata = lua_gettop(L);
    void* storage = lua_touserdata(L, userdata);
    const NativeProperty* speed = NativeProperty::find(L, userdata, "speed");
    const NativeProperty* position = NativeProperty::find(L, userdata, "position");
    const NativeProperty* tint = NativeProperty::find(L, userdata, "tint");
    const NativeProperty* depth = NativeProperty::find(L, userdata, "depth");
    const NativeProperty* scale = NativeProperty::find(L, userdata, "scale");
    ASSERT_TRUE(speed != nullptr && position != nullptr && tint != nullptr && depth != nullptr && scale != nullptr);
    EXPECT_EQ(position->kind, NativeProperty::Kind::Vector);
    EXPECT_EQ(tint->kind, NativeProperty::Kind::Color);
    EXPECT_EQ(depth->kind, NativeProperty::Kind::Number);
    EXPECT_EQ(NativeProperty::find(L, userdata, "visible"), nullptr);
    EXPECT_EQ(NativeProperty::find(L, userdata, "layer"), nullptr);

    NativeProperty::Values values{0.25F, 0.5F, 0.75F, 1.0F};
    ASSERT_TRUE(tint->write(storage, values));
    EXPECT_EQ(probe->tint, (math::Color{0.25F, 0.5F, 0.75F, 1.0F}));
    ASSERT_TRUE(position->write(storage, values));
    EXPECT_EQ(probe->position, (math::Vec2{0.25F, 0.5F}));

    // The userdata of a released object stays, and its properties no longer reach the object.
    static_cast<std::shared_ptr<Probe>*>(storage)->reset();
    for (const NativeProperty* property : {speed, depth, scale}) {
        EXPECT_FALSE(property->read(storage, values));
        EXPECT_FALSE(property->write(storage, values));
    }
    lua_pop(L, 1);
    EXPECT_NE(fixture.lua("return probeSpeed(probe)").find("This \"haylen.test.Probe\" was already released."), std::string::npos);

    // Values that are no userdata, and userdata of a type without native properties, have none.
    lua_newtable(L);
    EXPECT_EQ(NativeProperty::find(L, -1, "speed"), nullptr);
    lua_newuserdatauv(L, 0, 0);
    lua_newtable(L);
    lua_setmetatable(L, -2);
    EXPECT_EQ(NativeProperty::find(L, -1, "speed"), nullptr);
    lua_pop(L, 2);
}

// A binding tells which converter reads an argument, the way `intersects` tells a circle from a rectangle.
TEST(ConverterTest, TellsWhichValuesEachConverterReads) {
    test::EngineFixture fixture;
    lua_State* L = fixture.lua();
    const int flag = lua_gettop(L) + 1;
    const int hex = flag + 1;
    const int number = flag + 2;
    const int table = flag + 3;
    const int rect = flag + 4;
    const int json = flag + 5;
    const int none = flag + 6;
    lua_pushboolean(L, 1);
    lua_pushliteral(L, "#FF8000");
    lua_pushnumber(L, 2.5);
    lua_newtable(L);
    Stack::push(L, math::Rect{1.0F, 2.0F, 3.0F, 4.0F});
    Stack::push(L, core::Json{{"days", 3}});

    EXPECT_TRUE(Stack::is<bool>(L, flag));
    EXPECT_FALSE(Stack::is<bool>(L, number));
    EXPECT_TRUE(Stack::is<std::string>(L, hex));
    EXPECT_FALSE(Stack::is<std::string>(L, number));
    EXPECT_TRUE(Stack::is<std::string_view>(L, hex));
    EXPECT_FALSE(Stack::is<std::string_view>(L, table));
    EXPECT_TRUE(Stack::is<math::Rect>(L, rect));
    EXPECT_TRUE(Stack::is<math::Rect>(L, table));
    EXPECT_FALSE(Stack::is<math::Rect>(L, hex));
    EXPECT_TRUE(Stack::is<math::Color>(L, hex));
    EXPECT_TRUE(Stack::is<math::Color>(L, table));
    EXPECT_FALSE(Stack::is<math::Color>(L, rect));
    EXPECT_TRUE(Stack::is<core::Json>(L, number));
    EXPECT_FALSE(Stack::is<core::Json>(L, none));
    EXPECT_TRUE(Stack::is<std::optional<bool>>(L, none));
    EXPECT_FALSE(Stack::is<std::optional<bool>>(L, hex));
    EXPECT_EQ(Stack::read<core::Json>(L, json), (core::Json{{"days", 3}}));
    lua_settop(L, flag - 1);
}

// Every value of the enums that cross the Lua boundary has a name that reads back as the same value.
TEST(ConverterTest, NamesEveryValueOfTheEnumsOfTheEngine) {
    // clang-format off
    const auto namesEveryValue = []<NamedEnum Enum>(Enum last) {
        for (int value = 0; value <= static_cast<int>(last); ++value) {
            const auto original = static_cast<Enum>(value);
            if (EnumNames<Enum>::name(original).empty() || EnumNames<Enum>::fromName(EnumNames<Enum>::name(original)) != original) {
                return false;
            }
        }
        return true;
    };
    // clang-format on
    EXPECT_TRUE(namesEveryValue(graphics2d::Renderer::SortMode::Y));
    EXPECT_TRUE(namesEveryValue(graphics2d::ImageBlend::Pattern::PageTurn));
    EXPECT_TRUE(namesEveryValue(graphics2d::SceneTransition::Kind::Pixelate));
    EXPECT_TRUE(namesEveryValue(graphics2d::SceneTransition::Direction::DownRight));
    EXPECT_TRUE(namesEveryValue(platform::Window::Cursor::NotAllowed));
    EXPECT_TRUE(namesEveryValue(platform::Window::Passthrough::Regions));
    EXPECT_TRUE(namesEveryValue(input::InputDevice::Gamepad));
    EXPECT_TRUE(namesEveryValue(input::TouchPhase::Cancelled));
    EXPECT_TRUE(namesEveryValue(input::GamepadButton::DpadRight));
    EXPECT_TRUE(namesEveryValue(input::GamepadAxis::RightTrigger));
    EXPECT_TRUE(namesEveryValue(platform::Event::Type::MonitorsChanged));
    EXPECT_TRUE(namesEveryValue(platform::TextInput::Action::Dismissed));
}

TEST(WeakReferenceTest, RefersOnlyToTablesAndUserdata) {
    test::EngineFixture fixture;
    lua_State* L = fixture.lua();
    lua_newtable(L);
    const WeakReference table(L, -1);
    ASSERT_TRUE(table.push(L));
    EXPECT_TRUE(lua_rawequal(L, -1, -2));
    lua_pop(L, 2);

    // clang-format off
    lua_pushcfunction(L, [](lua_State* state) -> int {
        lua_pushinteger(state, 7);
        const WeakReference number(state, -1);
        return 0;
    });
    // clang-format on
    ASSERT_NE(lua_pcall(L, 0, 0, 0), LUA_OK);
    EXPECT_STREQ(lua_tostring(L, -1), "Only a table or a userdata can be referenced weakly, not number.");
    lua_pop(L, 1);
}

} // namespace haylen::lua
