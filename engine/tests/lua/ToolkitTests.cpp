#include <gtest/gtest.h>
#include <lua.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include "haylen/lua/Error.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "support/EngineFixture.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::lua {

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
            {"source/ads-kit/banner.lua", "return {}"},
            {"source/ads-kit/notes.txt", "Not a module."},
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
    EXPECT_EQ(lua_gettop(L), 0);

    Runtime::runReporting(L, [] { throw std::runtime_error("reported"); });
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "reported");
    EXPECT_TRUE(fixture.engine().getError()->getFrames().empty());
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
    EXPECT_EQ(fixture.lua("return table.concat(results, ' | ')"), "JSON value is nested too deeply to push to Lua. | Binary JSON values cannot be converted to Lua. | table");
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

    lua_pushstring(L, "kept");
    Reference reference(L, -1);
    lua_pop(L, 1);
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
    assigned.reset();
    assigned.reset();
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

    fixture.runLua("counted = setmetatable({}, {__len = function() error('never called') end})");
    lua_getglobal(L, "counted");
    EXPECT_TRUE(JsonConverter::read(L, -1).is_object());
    lua_settop(L, 0);

    for (const char* invalid : {"return (function() local cyclic = {} cyclic.self = cyclic return cyclic end)()", "return {callback = print}", "return {[true] = 1}"}) {
        ASSERT_EQ(luaL_dostring(L, invalid), LUA_OK);
        EXPECT_THROW((void)JsonConverter::read(L, -1), std::invalid_argument) << invalid;
        lua_settop(L, 0);
    }
    EXPECT_THROW(JsonConverter::push(L, core::Json::binary({1, 2})), std::invalid_argument);

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

} // namespace haylen::lua
