#include <gtest/gtest.h>

#include <lua.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include "haylen/core/Application.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/lua/Application.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

class NativeLuaTest : public ::testing::Test {
  protected:
    using Reporter = void (*)(std::int32_t value, const std::uint8_t* data, std::size_t size);

    // The C declarations of the test library, and a Lua function that tells whether it runs on the thread of the test, which drives the frames.
    static void prepare(test::EngineFixture& fixture) {
        lua_State* L = fixture.lua();
        lua_pushinteger(L, static_cast<lua_Integer>(std::hash<std::thread::id>{}(std::this_thread::get_id())));
        // clang-format off
        lua_pushcclosure(L, [](lua_State* state) -> int {
            lua_pushboolean(state, static_cast<lua_Integer>(std::hash<std::thread::id>{}(std::this_thread::get_id())) == lua_tointeger(state, lua_upvalueindex(1)) ? 1 : 0);
            return 1;
        }, 1);
        // clang-format on
        lua_setglobal(L, "isTestThread");

        fixture.runLua(R"(
            ffi = require('ffi')
            native = require('haylen.native')
            platform = require('haylen.platform')
            async = require('async')
            ffi.cdef[[
                typedef struct NativeTestPoint { int32_t x; int32_t y; } NativeTestPoint;
                typedef struct NativeTestRect { NativeTestPoint origin; float width; float height; } NativeTestRect;
                typedef void (*NativeTestVisitor)(int32_t index, const char* label);
                typedef void (*NativeTestReporter)(int32_t value, const uint8_t* data, size_t size);
                int32_t native_test_add(int32_t a, int32_t b);
                double native_test_scale(double value, double factor);
                const char* native_test_origin(void);
                NativeTestPoint native_test_point_add(NativeTestPoint a, NativeTestPoint b);
                void native_test_rect_grow(NativeTestRect* rect, float amount);
                void native_test_fill(uint8_t* buffer, size_t size, uint8_t seed);
                uint32_t native_test_checksum(const uint8_t* buffer, size_t size);
                int32_t native_test_visit(int32_t count, NativeTestVisitor visitor);
                void native_test_report_later(NativeTestReporter reporter, int32_t value);
            ]]
            lib = native.load('native_test')
        )");
    }

    [[nodiscard]] static Reporter pointerOf(test::EngineFixture& fixture, const char* callback) {
        lua_State* L = fixture.lua();
        lua_getglobal(L, callback);
        lua_getfield(L, -1, "pointer");
        auto* pointer = reinterpret_cast<Reporter>(lua_touserdata(L, -1));
        lua_pop(L, 2);
        return pointer;
    }
};

TEST_F(NativeLuaTest, CallsValuesStructsTextAndBuffersOfALoadedLibrary) {
    test::EngineFixture fixture;
    prepare(fixture);
    // clang-format off
    fixture.runLua(std::string(R"(
        local byPath = native.load([[)") + HAYLEN_NATIVE_TEST_LIBRARY + R"(]])
        local sum = lib.native_test_point_add(ffi.new('NativeTestPoint', {1, 2}), ffi.new('NativeTestPoint', {x = 10, y = 20}))
        local rect = ffi.new('NativeTestRect', {origin = {5, 5}, width = 10, height = 4})
        lib.native_test_rect_grow(rect, 2)
        local buffer = ffi.new('uint8_t[?]', 8)
        lib.native_test_fill(buffer, 8, 250)
        bytes = ffi.string(buffer, 8)
        checksum = lib.native_test_checksum(ffi.cast('const uint8_t*', buffer), 8)
        summary = table.concat({
            byPath.native_test_add(20, 22), lib.native_test_scale(1.5, 4), ffi.string(lib.native_test_origin()),
            sum.x, sum.y, rect.origin.x, rect.origin.y, rect.width, rect.height,
            tostring(native.available()), tostring(native.findSymbol('native_test_add') ~= nil), tostring(native.findSymbol('native_test_nowhere')),
        }, ' ')
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return summary"), "42 6.0 dynamic 11 22 3 3 14.0 8.0 true true nil");
    EXPECT_EQ(fixture.lua("return bytes == '\\250\\251\\252\\253\\254\\255\\0\\1'"), "true");
    std::uint32_t expected = 2166136261U;
    for (const std::uint8_t value : std::array<std::uint8_t, 8>{250, 251, 252, 253, 254, 255, 0, 1}) {
        expected = (expected ^ value) * 16777619U;
    }
    EXPECT_EQ(fixture.lua("return checksum"), std::to_string(expected));
}

TEST_F(NativeLuaTest, RunsFrameCallbacksAtOnceAndOthersAtTheNextFrame) {
    test::EngineFixture fixture;
    prepare(fixture);
    // clang-format off
    fixture.runLua(R"(
        visits = {}
        atOnce = native.callback('void (int32_t index, const char* label)', function(index, label) visits[#visits + 1] = 'frame:' .. index .. label end, {thread = 'frame'})
        later = native.callback('void (int32_t index, const char* label)', function(index, label) visits[#visits + 1] = 'any:' .. index .. label end)
        viaFfi = ffi.cast('NativeTestVisitor', function(index, label) visits[#visits + 1] = 'ffi:' .. index .. ffi.string(label) end)
        lib.native_test_visit(2, atOnce.pointer)
        lib.native_test_visit(2, later.pointer)
        lib.native_test_visit(1, viaFfi)
        afterCall = table.concat(visits, ',')
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return afterCall"), "frame:0zero,frame:1one,ffi:0zero");

    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(visits, ',')"), "frame:0zero,frame:1one,ffi:0zero,any:0zero,any:1one");

    fixture.runLua("later:free() later:free()");
    EXPECT_EQ(fixture.lua("return later.freed and not atOnce.freed"), "true");
    EXPECT_NE(fixture.lua("return later.pointer").find("was already released"), std::string::npos);
    EXPECT_NE(fixture.lua("native.callback('void (int)', print, {thread = 'main'})").find("is 'any' or 'frame'"), std::string::npos);
    EXPECT_NE(fixture.lua("native.callback('int (int)', print)").find("returns nothing"), std::string::npos);
    EXPECT_NE(fixture.lua("native.callback('void (int)', print, {threads = 'any'})").find("threads"), std::string::npos);
}

TEST_F(NativeLuaTest, DeliversCallsFromNativeThreadsOnTheFrameThread) {
    test::EngineFixture fixture;
    prepare(fixture);
    // clang-format off
    fixture.runLua(R"(
        reports = {}
        local function record(value, data, size)
            reports[#reports + 1] = value .. ':' .. #data .. ':' .. data:byte(1) .. data:byte(4) .. ':' .. size .. ':' .. tostring(isTestThread())
        end
        reporter = native.callback('void (int32_t value, const uint8_t data[size], size_t size)', record)
        framed = native.callback('void (int32_t value, const uint8_t data[size], size_t size)', record, {thread = 'frame'})
        lib.native_test_report_later(reporter.pointer, 42)
        lib.native_test_report_later(framed.pointer, 7)
    )");
    // clang-format on

    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #reports") == "2"; }));
    fixture.runLua("table.sort(reports)");
    EXPECT_EQ(fixture.lua("return table.concat(reports, ',')"), "42:4:14:4:true,7:4:14:4:true");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST_F(NativeLuaTest, ReportsErrorsOfCallbacksOnTheErrorScreen) {
    {
        test::EngineFixture fixture;
        prepare(fixture);
        fixture.runLua("failing = native.callback('void (int32_t index, const char* label)', function() error('callback failed') end, {thread = 'frame'}) visited = lib.native_test_visit(1, failing.pointer)");
        EXPECT_EQ(fixture.lua("return visited"), "1") << "native code keeps running after the error";
        ASSERT_NE(fixture.engine().getError(), nullptr);
        EXPECT_NE(std::string(fixture.engine().getError()->what()).find("callback failed"), std::string::npos);
    }

    test::EngineFixture fixture;
    prepare(fixture);
    fixture.runLua("negative = native.callback('void (int32_t value, const uint8_t data[value], size_t size)', function() called = true end) lib.native_test_report_later(negative.pointer, -3)");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.engine().getError() != nullptr; }));
    EXPECT_NE(std::string(fixture.engine().getError()->what()).find("negative length -3 for data"), std::string::npos);
    EXPECT_EQ(fixture.lua("return called"), "nil");
}

TEST_F(NativeLuaTest, DropsCallsThatArriveAfterTheAppStopped) {
    Reporter queued = nullptr;
    Reporter immediate = nullptr;
    const std::array<std::uint8_t, 2> data{1, 2};
    {
        test::EngineFixture fixture;
        prepare(fixture);
        fixture.runLua(R"(
            received = 0
            queued = native.callback('void (int32_t value, const uint8_t data[size], size_t size)', function() received = received + 1 end)
            immediate = native.callback('void (int32_t value, const uint8_t data[size], size_t size)', function() received = received + 1 end, {thread = 'frame'})
        )");
        queued = pointerOf(fixture, "queued");
        immediate = pointerOf(fixture, "immediate");
        immediate(1, data.data(), data.size());
        queued(2, data.data(), data.size());
        EXPECT_EQ(fixture.lua("return received"), "1");
    }

    // The app that created the callbacks is gone, so calls from any thread do nothing, and a new app never sees them.
    queued(3, data.data(), data.size());
    immediate(4, data.data(), data.size());
    // clang-format off
    std::thread([&] {
        queued(5, data.data(), data.size());
        immediate(6, data.data(), data.size());
    }).join();
    // clang-format on

    test::EngineFixture restarted;
    restarted.runLua("received = 0");
    restarted.frames(2);
    EXPECT_EQ(restarted.lua("return received"), "0");
    EXPECT_EQ(restarted.engine().getError(), nullptr);
}

TEST_F(NativeLuaTest, GivesLibrariesTheInterfaceOfTheEngine) {
    test::EngineFixture fixture({{"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "plugins": {"native-test": {}}})"}, {"plugins/native-test/plugin.json", R"({"id": "native-test", "version": "1.0.0"})"}});
    prepare(fixture);
    fixture.runLua("testPlugin = platform.plugin('native-test') nativeBefore = testPlugin.native");
    // clang-format off
    fixture.runLua(R"(
        cancelled = {}
        platform.on('native_test.cancelled', function(payload) cancelled[#cancelled + 1] = payload.call end)
        native.load('native_test', {init = 'native_test_haylen_init'})
        async.spawn(function()
            echo = platform.call('native_test.echo', {word = 'hi'}):await()
            local _, failure = platform.call('native_test.fail'):await()
            failed = failure
            local timed = platform.call('native_test.wait', nil, {timeout = 0.05})
            timedId = timed.id
            local _, timeout = timed:await()
            local given = platform.call('native_test.wait')
            givenId = given.id
            local stopped = given:cancel()
            local _, cancel = given:await()
            givenUp = table.concat({timeout.code, cancel.code, tostring(stopped), tostring(given:cancel()), tostring(given.done)}, ' ')
        end)
    )");
    // clang-format on

    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return givenUp ~= nil and #cancelled == 2") == "true"; }));
    EXPECT_EQ(fixture.lua("return echo.echo.word .. ' ' .. tostring(echo.thread)"), "hi true");

    // The init function declared the library the native part of the plugin, which the handle made before sees too.
    EXPECT_EQ(fixture.lua("return tostring(nativeBefore) .. ' ' .. tostring(testPlugin.native) .. ' ' .. tostring(platform.plugins()[1].native)"), "false true true");

    // The library announced itself with a retained event, which waits for a listener that connects late.
    fixture.runLua("platform.on('native_test.ready', function(payload) ready = payload end)");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return ready ~= nil") == "true"; }));
    EXPECT_EQ(fixture.lua("return ready.version .. ' ' .. ready.origin"), "4 dynamic");
    EXPECT_EQ(fixture.lua("return failed.message .. ' ' .. failed.code .. ' ' .. failed.data.reason"), "The native test failed on purpose. native_test_failure requested");
    EXPECT_EQ(fixture.lua("return tostring(failed) .. ' | ' .. ('error: ' .. failed) .. ' | ' .. (failed .. '!')"), "The native test failed on purpose. | error: The native test failed on purpose. | The native test failed on purpose.!");

    // The cancel notices reached the handler of the library, which answered them with events.
    EXPECT_EQ(fixture.lua("return givenUp"), "timeout cancelled true false true");
    EXPECT_EQ(fixture.lua("return cancelled[1] == timedId and cancelled[2] == givenId"), "true");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST_F(NativeLuaTest, ExchangesBytesBatchesAndStreamsWithLibraries) {
    test::EngineFixture fixture({{"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "plugins": {"native-test": {}}})"}, {"plugins/native-test/plugin.json", R"({"id": "native-test", "version": "1.0.0"})"}});
    prepare(fixture);
    // clang-format off
    fixture.runLua(R"(
        ffi.cdef[[
            void native_test_push_frame_later(int32_t width, int32_t height, int32_t seed);
            void native_test_push_tone_later(int32_t count, int32_t value);
            int32_t native_test_reopen_refused(void);
        ]]
        collections = require('haylen.collections')
        native.load('native_test', {init = 'native_test_haylen_init'})
        test = platform.plugin('native-test')
        missing = test:videoStream('pattern') == nil and test:audioStream('tone') == nil
        bursts = {}
        platform.on('native_test.burst', function(list) bursts[#bursts + 1] = list end)
        async.spawn(function()
            bytes = platform.call('native_test.echoBytes', {data = platform.bytes('\0\1\2\255')}):await()
            platform.call('native_test.burst', {count = 100}):await()
        end)
        lib.native_test_push_frame_later(4, 2, 10)
        lib.native_test_push_tone_later(800, 16384)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return bytes ~= nil and #bursts > 0 and test:videoStream('pattern') ~= nil and test:audioStream('tone') ~= nil") == "true"; }));
    EXPECT_EQ(fixture.lua("return missing"), "true");

    // The bytes crossed to the library and back as buffers, and the batched events of one frame arrived as one list.
    EXPECT_EQ(fixture.lua(R"(return table.concat({bytes.size, tostring(bytes.same == '\0\1\2\255'), tostring(bytes.reversed == '\255\2\1\0')}, ' '))"), "4 true true");
    EXPECT_EQ(fixture.lua("return table.concat({#bursts, #bursts[1], bursts[1][1].index, bursts[1][100].index, string.byte(bursts[1][100].byte)}, ' ')"), "1 100 0 99 99");

    // The frame of the library thread reaches the texture at the start of a frame, and the samples reach the ring of the audio stream.
    fixture.runLua("video = test:videoStream('pattern') texture = video.texture audio = test:audioStream('tone') buffer = collections.newFloatBuffer(1000)");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return video.frameCount") == "1"; }));
    EXPECT_EQ(fixture.lua("return table.concat({video.width, video.height, texture.width, texture.height, video.timestamp, tostring(video.texture == texture)}, ' ')"), "4 2 4 2 1.0 true");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return audio:read(buffer)") == "800"; }));
    EXPECT_EQ(fixture.lua("return table.concat({audio.sampleRate, audio.channels, buffer[1], buffer[800], buffer[801]}, ' ')"), "8000 1 0.5 0.5 0.0");
    EXPECT_EQ(fixture.lua("return lib.native_test_reopen_refused()"), "1");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST_F(NativeLuaTest, HandsTheErrorsThatStopAppsToLibraries) {
    const std::string declarations = "ffi.cdef[[ int32_t native_test_error_count(void); const char* native_test_last_error(void); ]] before = lib.native_test_error_count()";
    {
        test::EngineFixture fixture;
        prepare(fixture);
        fixture.runLua("native.load('native_test', {init = 'native_test_haylen_init'}) native.load('native_test', {init = 'native_test_haylen_init'}) " + declarations);
        fixture.runLua("require('haylen').reportError('the first app failed')");
        ASSERT_NE(fixture.engine().getError(), nullptr);
        EXPECT_EQ(fixture.lua("return lib.native_test_error_count() - before"), "1") << "a handler that registered twice hears an error once";
        EXPECT_EQ(core::Json::parse(fixture.lua("return ffi.string(lib.native_test_last_error())")), fixture.engine().getError()->toJson());
    }

    // The handler belongs to the process, so it hears the errors of the next app without registering again.
    test::EngineFixture restarted;
    prepare(restarted);
    restarted.runLua(declarations);
    restarted.runLua("require('haylen').reportError('the next app failed')");
    EXPECT_EQ(restarted.lua("return lib.native_test_error_count() - before"), "1");
    EXPECT_EQ(core::Json::parse(restarted.lua("return ffi.string(lib.native_test_last_error())")).at("message"), "the next app failed");
}

TEST_F(NativeLuaTest, ExplainsWhyALibraryDidNotLoad) {
    test::EngineFixture fixture;
    prepare(fixture);
    EXPECT_NE(fixture.lua("native.load('native_nowhere')").find("The native library 'native_nowhere' could not be loaded. These are the places it searched:"), std::string::npos);
    EXPECT_NE(fixture.lua("native.load('native_test', {init = 'native_test_absent'})").find("The native library 'native_test' has no function 'native_test_absent'."), std::string::npos);
    EXPECT_NE(fixture.lua("native.load('native_test', {init = 'native_test_failing_init'})").find("The function 'native_test_failing_init' of the native library 'native_test' failed with code 7."), std::string::npos);
    EXPECT_NE(fixture.lua("native.load('native_test', {inti = 'x'})").find("inti"), std::string::npos);
    EXPECT_EQ(fixture.lua("return ffi.istype('NativeTestPoint', ffi.new('NativeTestPoint')) and native.load('native_test', {global = true}) ~= nil"), "true");
}

class AppPluginTest : public ::testing::Test {
  protected:
    // A plugin of an app with a Lua module of its own, which tells how many updates the plugin saw.
    class CounterPlugin final : public plugins::Plugin {
      public:
        [[nodiscard]] std::string_view getName() const noexcept override {
            return "counter";
        }
        void update(core::Engine&, float) override {
            ++updates;
        }
        void installLua(core::Engine&, lua_State* L) override {
            lua::Binding::preload(L, "game.counter", &open);
        }

      private:
        static int getUpdates(lua_State* L) {
            lua_pushinteger(L, lua::Runtime::getEngine(L).getPlugin<CounterPlugin>().updates);
            return 1;
        }

        static int open(lua_State* L) {
            const luaL_Reg functions[] = {{"updates", &getUpdates}, {nullptr, nullptr}};
            lua::Binding::newModule(L, functions);
            return 1;
        }

        int updates = 0;
    };

    // Adds the plugin before the Lua of the app runs, the way a C++ app keeps a Lua application.
    class PluginApplication final : public core::Application {
      public:
        void start(core::Engine& engine) override {
            engine.addPlugin(std::make_unique<CounterPlugin>());
            application.start(engine);
        }
        void stop(core::Engine& engine) override {
            application.stop(engine);
        }

      private:
        lua::Application application;
    };
};

TEST_F(AppPluginTest, RunsAPluginOfTheAppWithItsLuaModule) {
    test::EngineFixture fixture({{"source/main.lua", "counter = require('game.counter') before = counter.updates()"}}, std::make_unique<PluginApplication>());
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return before .. ' ' .. counter.updates()"), "0 3");
    EXPECT_NE(fixture.engine().getPlugins().find("counter"), nullptr);
}

} // namespace haylen::platform
