#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "haylen/core/Engine.hpp"
#include "haylen/core/TweenManager.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::core {

TEST(TweenLuaTest, TweensEveryKindOfValue) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        tween = require('haylen.tween')
        m = require('haylen.math')
        box = {x = 0, angle = math.rad(350), score = 0, label = '', offset = m.vec2(0, 0), tint = m.color(1, 0, 0, 1), hue = m.color(1, 0, 0, 1), position = m.vec2(5, 5)}
        original = box.position
        tween.to(box, 1, {x = 100, angle = math.rad(10), score = 1000, label = 'hello', offset = {10, 20}, tint = '#FF0000FF', ['position.x'] = 25}, {angles = {'angle'}, integers = {'score'}})
        tween.to(box, 1, {hue = '#FF0000FF'}, {colorSpace = 'hsv'})
        faded = {alpha = 1}
        tween.from(faded, 1, {alpha = 0})
        moved = {x = 5}
        tween.by(moved, 1, {x = 10})
        both = {x = 0}
        tween.fromTo(both, 1, {x = 100}, {x = 200})
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return faded.alpha"), "0.0");
    fixture.frames(2, 0.25);
    EXPECT_EQ(fixture.lua("return box.x .. ' ' .. string.format('%.4f', box.angle) .. ' ' .. box.score .. ' ' .. box.label"), "50.0 " + std::string(fixture.lua("return string.format('%.4f', math.rad(360))")) + " 500.0 hel");
    EXPECT_EQ(fixture.lua("return box.offset.x .. ',' .. box.offset.y .. ' ' .. box.position.x .. ',' .. box.position.y .. ' ' .. original.x"), "5.0,10.0 15.0,5.0 5.0");
    EXPECT_EQ(fixture.lua("return string.format('%.2f %.2f %.2f | %.2f %.2f %.2f', box.tint.r, box.tint.g, box.tint.b, box.hue.r, box.hue.g, box.hue.b)"), "0.50 0.00 0.50 | 1.00 0.00 1.00");
    EXPECT_EQ(fixture.lua("return faded.alpha .. ' ' .. moved.x .. ' ' .. both.x"), "0.5 10.0 150.0");
    fixture.frames(2, 0.25);
    EXPECT_EQ(fixture.lua("return box.label .. ' ' .. moved.x .. ' ' .. tween.size()"), "hello 15.0 0");
}

TEST(TweenLuaTest, RunsCallbacksAndControlsHandles) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        tween = require('haylen.tween')
        box = {x = 0}
        log = {}
        handle = tween.to(box, 1, {x = 100}, {
            delay = 0.25,
            repeatCount = 1,
            loopMode = 'yoyo',
            ease = 'linear',
            onStart = function() log[#log + 1] = 'start' end,
            onUpdate = function(p) log[#log + 1] = string.format('%.2f', p) end,
            onLoop = function(loop) log[#log + 1] = 'loop ' .. loop end,
            onComplete = function() log[#log + 1] = 'done' end,
            onKill = function() log[#log + 1] = 'kill' end,
        })
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return handle.duration .. ' ' .. handle.totalDuration .. ' ' .. handle.delay .. ' ' .. tostring(handle.playing)"), "1.0 2.0 0.25 true");
    fixture.frames(3, 0.25);
    EXPECT_EQ(fixture.lua("return box.x .. ' ' .. handle.time .. ' ' .. handle.progress"), "50.0 0.5 0.25");
    fixture.frames(4, 0.25);
    EXPECT_EQ(fixture.lua("return box.x .. ' ' .. table.concat(log, ',')"), "50.0 start,0.00,0.25,0.50,0.75,loop 1,1.00,0.75,0.50");
    fixture.frames(2, 0.25);
    EXPECT_EQ(fixture.lua("return box.x .. ' ' .. log[#log - 1] .. ' ' .. log[#log] .. ' ' .. tostring(handle.alive) .. ' ' .. tostring(handle.completed)"), "0.0 done kill false true");

    // clang-format off
    fixture.runLua(R"(
        toggle = tween.to(box, 2, {x = 20}, {autoKill = false, paused = true})
        results = {}
        require('async').spawn(function() results[#results + 1] = tostring(toggle:wait():await()) end)
    )");
    // clang-format on
    fixture.frames(2, 0.25);
    EXPECT_EQ(fixture.lua("return box.x .. ' ' .. tostring(toggle.paused) .. ' ' .. tostring(toggle.playing)"), "0.0 true false");
    fixture.runLua("toggle:play().timeScale = 2");
    fixture.frames(1, 0.25);
    EXPECT_EQ(fixture.lua("return box.x .. ' ' .. toggle.timeScale"), "5.0 2.0");
    fixture.runLua("toggle.progress = 0.75");
    EXPECT_EQ(fixture.lua("return box.x"), "15.0");
    fixture.runLua("toggle.time = 0.5");
    EXPECT_EQ(fixture.lua("return box.x"), "5.0");
    fixture.runLua("toggle:complete()");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return box.x .. ' ' .. table.concat(results, ',') .. ' ' .. tostring(toggle.alive)"), "20.0 true true");
    fixture.runLua("toggle:reverse()");
    EXPECT_EQ(fixture.lua("return tostring(toggle.reversed)"), "true");
    fixture.frames(4, 0.25);
    EXPECT_EQ(fixture.lua("return box.x .. ' ' .. tostring(toggle.completed)"), "0.0 true");
    fixture.runLua("toggle:restart():pause() toggle:resume() stopped = toggle:wait() toggle:kill()");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(toggle.alive) .. ' ' .. tween.size()"), "false 0");
}

TEST(TweenLuaTest, AwaitsCompletionAndKills) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        tween = require('haylen.tween')
        async = require('async')
        box = {x = 0}
        spin = tween.to(box, 1, {x = 10}, {repeatCount = -1, loopMode = 'yoyo', ease = 'sineInOut'})
        results = {}
        local function record(value) results[#results + 1] = tostring(value) end
        async.spawn(function()
            local moved = tween.to(box, 0.5, {x = 5})
            spin:kill()
            record(moved:wait():await())
            local stopped = tween.to(box, 5, {x = 50})
            async.spawn(function() record(stopped:wait():await()) end)
            stopped:kill()
            record(stopped:wait():await())
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #results") == "3"; }));
    EXPECT_EQ(fixture.lua("return table.concat(results, ',')"), "true,false,false");
    fixture.runLua("stopped = nil local all = tween.to(box, 1, {x = 0}) async.spawn(function() stopped = all:wait():await() end) tween.killAll()");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tween.size() .. ' ' .. tostring(stopped)"), "0 false");

    // A kept tween that already completed waits for its next completion, and a kill settles the wait with false.
    fixture.runLua("kept = tween.to(box, 0.1, {x = 1}, {autoKill = false})");
    fixture.frames(1, 0.25);
    fixture.runLua("late = nil async.spawn(function() late = kept:wait():await() end) kept:kill()");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(kept.completed) .. ' ' .. tostring(late)"), "true false");
}

TEST(TweenLuaTest, BuildsTimelinesAndStaggers) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        tween = require('haylen.tween')
        box = {x = 0, y = 0}
        log = {}
        line = tween.timeline({onStep = function(step) log[#log + 1] = 'step ' .. step end, onComplete = function() log[#log + 1] = 'done' end})
        line:append(tween.to(box, 1, {x = 10}))
            :join(tween.to(box, 0.5, {y = 5}))
            :append(function() log[#log + 1] = 'call ' .. box.x end)
            :addLabel('rest')
            :append(0.5)
            :append(tween.to(box, 1, {x = 0}))
            :insert(0.25, function() log[#log + 1] = 'early' end)
            :insert('rest', tween.to(box, 0.5, {y = 0}))
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return line.duration .. ' ' .. line.size .. ' ' .. tween.size()"), "2.5 7 1");
    fixture.frames(3, 0.25);
    EXPECT_EQ(fixture.lua("return box.x .. ' ' .. box.y"), "7.5 5.0");
    fixture.frames(7, 0.25);
    EXPECT_EQ(fixture.lua("return box.x .. ' ' .. box.y .. ' ' .. table.concat(log, ',')"), "0.0 0.0 early,call 10.0,step 1,step 2,step 3,step 4,done");

    // clang-format off
    fixture.runLua(R"(
        rows = {{v = 0}, {v = 0}, {v = 0}}
        wave = tween.stagger(rows, 0.5, function(row, index) return tween.to(row, 1, {v = index}) end, {origin = 'end', tag = 'wave'})
    )");
    // clang-format on
    fixture.frames(2, 0.25);
    EXPECT_EQ(fixture.lua("return rows[1].v .. ' ' .. rows[2].v .. ' ' .. rows[3].v .. ' ' .. wave.duration .. ' ' .. wave.tag"), "0 0.0 1.5 2.0 wave");
    fixture.runLua("wave:seek(2)");
    EXPECT_EQ(fixture.lua("return rows[1].v .. ' ' .. rows[2].v .. ' ' .. rows[3].v"), "1.0 2.0 3.0");

    fixture.runLua("marked = tween.timeline({autoKill = false}):append(tween.to(box, 1, {x = 4})):addLabel('half', 0.5)");
    fixture.runLua("marked:seek('half')");
    EXPECT_EQ(fixture.lua("return box.x"), "2.0");
    EXPECT_NE(fixture.lua("tween.timeline():append(tween.to(box, 1, {x = 1}, {speedBased = true}))").find("speed-based"), std::string::npos);
    EXPECT_NE(fixture.lua("marked:insert('missing', function() end)").find("no label named 'missing'"), std::string::npos);
    EXPECT_NE(fixture.lua("tween.stagger({{v = 1}}, 1, function() return 5 end)").find("haylen.Tween expected"), std::string::npos);
}

TEST(TweenLuaTest, PlaysReadyMadeTweens) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        tween = require('haylen.tween')
        m = require('haylen.math')
        sprite = {x = 0, y = 0, scaleX = 1, scaleY = 1, rotation = math.rad(350), color = m.color(1, 1, 1, 1)}
        tween.move(sprite, 1, {100, 50})
        tween.scale(sprite, 1, 3)
        tween.rotate(sprite, 1, math.rad(10))
        tween.fade(sprite, 1, 0)
        hopper = {x = 0, y = 0}
        tween.jump(hopper, 1, {100, 0}, {power = 40, jumps = 2})
        walker = {x = 0, y = 0, rotation = 0}
        tween.path(walker, 1, {{10, 0}, {10, 30}}, {curved = false, orient = true})
        curve = {position = m.vec2(0, 0)}
        tween.bezier(curve, 1, {{0, 100}, {100, 0}}, {field = 'position'})
        light = {alpha = 1}
        tween.blink(light, 1, 2, {field = 'alpha'})
        tinted = {color = m.color(1, 1, 1, 1)}
        tween.tint(tinted, 1, '#FF000000')
    )");
    // clang-format on
    fixture.frames(1, 0.25);
    EXPECT_EQ(fixture.lua("return hopper.x .. ',' .. hopper.y .. ' ' .. walker.x .. ',' .. walker.y .. ' ' .. light.alpha"), "25.0,-40.0 10.0,0.0 0.0");
    fixture.frames(1, 0.25);
    EXPECT_EQ(fixture.lua("return sprite.x .. ',' .. sprite.y .. ' ' .. sprite.scaleX .. ',' .. sprite.scaleY .. ' ' .. string.format('%.4f', sprite.rotation) .. ' ' .. sprite.color.a .. ' ' .. curve.position.y .. ' ' .. light.alpha"), "50.0,25.0 2.0,2.0 " + fixture.lua("return string.format('%.4f', math.rad(360))") + " 0.5 50.0 1.0");
    fixture.frames(2, 0.25);
    EXPECT_EQ(fixture.lua("return walker.x .. ',' .. walker.y .. ' ' .. string.format('%.3f', walker.rotation) .. ' ' .. light.alpha .. ' ' .. tinted.color.r .. ' ' .. tinted.color.g"), "10.0,30.0 " + fixture.lua("return string.format('%.3f', math.pi / 2)") + " 1.0 0.0 0.0");

    // clang-format off
    fixture.runLua(R"(
        shaken = {x = 10, y = 10}
        tween.shake(shaken, 1, 5, {vibrato = 6, seed = 3})
        punched = {offset = 0}
        tween.punch(punched, 1, 8, {field = 'offset', vibrato = 3})
        samples = {}
        tween.to({}, 1, {}, {onUpdate = function() samples[#samples + 1] = shaken.x ~= 10 or shaken.y ~= 10 end})
    )");
    // clang-format on
    fixture.frames(2, 0.25);
    EXPECT_EQ(fixture.lua("return tostring(samples[1] or samples[2])"), "true");
    fixture.frames(2, 0.25);
    EXPECT_EQ(fixture.lua("return shaken.x .. ',' .. shaken.y .. ' ' .. string.format('%.3f', punched.offset)"), "10.0,10.0 0.000");

    EXPECT_NE(fixture.lua("tween.path({v = 1}, 1, {{1, 1}}, {field = 'v'})").find("needs a Vec2 field"), std::string::npos);
    EXPECT_NE(fixture.lua("tween.bezier({x = 0, y = 0}, 1, {{1, 1}})").find("one or two control points"), std::string::npos);
    EXPECT_NE(fixture.lua("tween.move({x = 0, y = 0}, 1, {1, 1}, {field = {'x'}})").find("two number field names"), std::string::npos);
    EXPECT_NE(fixture.lua("tween.blink({p = m.vec2(1, 1)}, 1, 2, {field = 'p'})").find("hidden option"), std::string::npos);
}

TEST(TweenLuaTest, AnimatesNativePropertiesWithoutLua) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        tween = require('haylen.tween')
        m = require('haylen.math')
        camera = require('haylen.graphics2d').newCamera()
        point = m.vec2(0, 0)
        tween.to(camera, 1, {zoom = {3, 5}, ['position.x'] = 40, rotation = 1})
        tween.move(point, 1, {10, 20})
    )");
    // clang-format on
    fixture.frames(2, 0.25);
    EXPECT_EQ(fixture.lua("return camera.zoom.x .. ',' .. camera.zoom.y .. ' ' .. camera.position.x .. ' ' .. camera.rotation .. ' ' .. point.x .. ',' .. point.y"), "2.0,3.0 20.0 0.5 5.0,10.0");

    // A collected target stops its tweens before they write again.
    fixture.runLua("point = nil collectgarbage() collectgarbage()");
    fixture.frames(1, 0.25);
    fixture.frames(2, 0.25);
    EXPECT_EQ(fixture.lua("return tween.size()"), "0");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(TweenLuaTest, TiesTweensToTargetsOwnersAndTags) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        tween = require('haylen.tween')
        scene = require('haylen.scene')
        tween.to({x = 0}, 1, {x = 1})
        collectgarbage() collectgarbage()
        level = {}
        box = {x = 0, y = 0}
        owned = tween.to(box, 10, {x = 10}, {owner = level})
        scene.push(level)
    )");
    // clang-format on
    fixture.frames(1, 0.25);
    EXPECT_EQ(fixture.lua("return tween.size() .. ' ' .. tostring(owned.alive)"), "1 true");
    fixture.runLua("scene.pop()");
    fixture.frames(1, 0.25);
    EXPECT_EQ(fixture.lua("return tostring(owned.alive) .. ' ' .. string.format('%.2f', box.x)"), "false 0.50");

    // An owner that is collected ends its tweens at the end of the frame, and the callbacks it held went away with it.
    fixture.runLua("local owner = {} held = tween.to(box, 10, {y = 10}, {owner = owner, onKill = function() killed = true end}) owner = nil collectgarbage() collectgarbage()");
    fixture.frames(1, 0.25);
    EXPECT_EQ(fixture.lua("return tostring(killed) .. ' ' .. tostring(held.alive)"), "nil false");

    // clang-format off
    fixture.runLua(R"(
        first = tween.to(box, 1, {x = 5, y = 5})
        second = tween.to(box, 1, {x = 9}, {overwrite = true})
        other = {v = 0}
        tagged = tween.to(other, 1, {v = 1}, {tag = 'hud'})
        tween.setTimeScale('hud', 2)
    )");
    // clang-format on
    fixture.frames(1, 0.25);
    EXPECT_EQ(fixture.lua("return tostring(first.alive) .. ' ' .. other.v .. ' ' .. tween.timeScale('hud')"), "true 0.5 2.0");
    fixture.runLua("tween.pauseTag('hud')");
    fixture.frames(1, 0.25);
    EXPECT_EQ(fixture.lua("return other.v"), "0.5");
    fixture.runLua("tween.resumeTag('hud') tween.completeTag('hud')");
    EXPECT_EQ(fixture.lua("return other.v .. ' ' .. tostring(tagged.alive)"), "1.0 false");
    fixture.runLua("tween.killTag('none') tween.killTarget(box)");
    EXPECT_EQ(fixture.lua("return tostring(first.alive) .. ' ' .. tostring(second.alive) .. ' ' .. tween.size()"), "false false 0");
}

TEST(TweenLuaTest, RunsByProcessModeAndTime) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        haylen = require('haylen')
        tween = require('haylen.tween')
        a, b, c, d = {v = 0}, {v = 0}, {v = 0}, {v = 0}
        tween.to(a, 1, {v = 1})
        tween.to(b, 1, {v = 1}, {processMode = 'whenPaused'})
        tween.to(c, 1, {v = 1}, {processMode = 'always', unscaled = true})
        tween.to(d, 1, {v = 1}, {fixedStep = true, speedBased = false})
        haylen.setTimeScale(0.5)
        haylen.setPaused(true)
    )");
    // clang-format on
    fixture.frames(2, 0.25);
    EXPECT_EQ(fixture.lua("return a.v .. ' ' .. b.v .. ' ' .. c.v .. ' ' .. d.v"), "0 0.25 0.5 0");
    fixture.runLua("haylen.setPaused(false) haylen.setTimeScale(1)");
    fixture.frames(1, 0.25);
    EXPECT_EQ(fixture.lua("return a.v .. ' ' .. b.v .. ' ' .. string.format('%.2f', d.v)"), "0.25 0.25 0.25");
    EXPECT_NE(fixture.lua("tween.to(a, 1, {v = 1}, {processMode = 'sometimes'})").find("sometimes"), std::string::npos);

    fixture.runLua("fast = {v = 0} ship = tween.to(fast, 100, {v = 50}, {speedBased = true})");
    fixture.frames(1, 0.25);
    EXPECT_EQ(fixture.lua("return fast.v .. ' ' .. ship.duration"), "25.0 0.5");
}

TEST(TweenLuaTest, EasesWithEveryKindOfCurve) {
    test::EngineFixture fixture;
    fixture.runLua("m = require('haylen.math') tween = require('haylen.tween')");
    EXPECT_EQ(fixture.lua("return string.format('%.3f', m.ease('quadIn', 0.5))"), "0.250");
    EXPECT_EQ(fixture.lua("return m.ease({steps = 4}, 0.3) .. ' ' .. m.ease({steps = 4, position = 'start'}, 0.3)"), "0.25 0.5");
    EXPECT_EQ(fixture.lua("return string.format('%.3f', m.ease({cubicBezier = {0.25, 0.1, 0.25, 1}}, 0.5))"), "0.802");
    EXPECT_EQ(fixture.lua("return m.ease({points = {0, 1, 0}}, 0.25) .. ' ' .. string.format('%.2f', m.ease({points = {{0, 0}, {0.5, 0.2}, {1, 1}}}, 0.75))"), "0.5 0.60");
    EXPECT_EQ(fixture.lua("return m.ease(function(t) return t * t * t end, 0.5)"), "0.125");
    EXPECT_EQ(fixture.lua("return tostring(m.ease({curve = 'backOut', overshoot = 3}, 0.6) > m.ease('backOut', 0.6))"), "true");
    EXPECT_EQ(fixture.lua("return tostring(m.ease({curve = 'elasticOut', amplitude = 2, period = 0.5}, 0.2) ~= m.ease('elasticOut', 0.2))"), "true");
    EXPECT_NE(fixture.lua("m.ease({curve = 'quadIn', overshoot = 2}, 0.5)").find("overshoot only applies"), std::string::npos);
    EXPECT_NE(fixture.lua("m.ease({cubicBezier = {2, 0, 0, 1}}, 0.5)").find("between 0 and 1"), std::string::npos);
    EXPECT_NE(fixture.lua("m.ease({speed = 1}, 0.5)").find("Unknown option 'speed'"), std::string::npos);
    EXPECT_NE(fixture.lua("m.ease({steps = 4294967297}, 0.5)").find("The option 'steps'"), std::string::npos);
    EXPECT_NE(fixture.lua("m.ease(function() return 'x' end, 0.5)").find("must return a number"), std::string::npos);

    fixture.runLua("box = {x = 0} tween.to(box, 1, {x = 10}, {ease = {steps = 2}})");
    fixture.frames(1, 0.25);
    EXPECT_EQ(fixture.lua("return box.x"), "0.0");
    fixture.frames(1, 0.25);
    EXPECT_EQ(fixture.lua("return box.x"), "5.0");
}

TEST(TweenLuaTest, ReportsMistakes) {
    test::EngineFixture fixture;
    fixture.runLua("tween = require('haylen.tween') box = {x = 0, name = {}}");
    EXPECT_NE(fixture.lua("tween.to(box, 1, {x = 1}, {speed = 2})").find("Unknown option 'speed'"), std::string::npos);
    EXPECT_NE(fixture.lua("tween.to(box, 1, {name = 'x'})").find("not a number, a Vec2, a Color or a text"), std::string::npos);
    EXPECT_NE(fixture.lua("tween.to(box, 1, {x = 'far'})").find("must be a number"), std::string::npos);
    EXPECT_NE(fixture.lua("tween.to(box, 1, {[1] = 2})").find("tween fields are named by strings"), std::string::npos);
    EXPECT_NE(fixture.lua("tween.to(box, 0, {x = 1})").find("positive duration"), std::string::npos);
    EXPECT_NE(fixture.lua("tween.to(box, 1, {x = 1}, {onUpdate = 3})").find("must be a function"), std::string::npos);
    EXPECT_NE(fixture.lua("tween.to(box, 1, {['a..b'] = 1})").find("empty part"), std::string::npos);
    EXPECT_NE(fixture.lua("tween.to(box, 1, {['x.y'] = 1})").find("part of its path is number"), std::string::npos);
    EXPECT_NE(fixture.lua("tween.fromTo(box, 1, {x = 1}, {})").find("needs an end value for the field 'x'"), std::string::npos);
    EXPECT_NE(fixture.lua("tween.to(5, 1, {})").find("must be a table or a userdata"), std::string::npos);

    // clang-format off
    fixture.runLua(R"(
        target = setmetatable({}, {__index = function() return 0 end, __newindex = function() error('setter refused') end})
        tween.to(target, 1, {x = 5})
    )");
    // clang-format on
    fixture.frames(2);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(fixture.engine().getError()->what()).find("setter refused"), std::string::npos);
}

TEST(LuaThreadTest, CallbacksRegisteredInCoroutinesRunAfterTheCoroutineEnds) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        async = require('async')
        timer = require('haylen.timer')
        tween = require('haylen.tween')
        fired = {}
        box = {x = 0}
        async.spawn(function()
            timer.after(0.1, function() fired[#fired + 1] = 'timer' end)
            tween.to(box, 0.1, {x = 1}, {onComplete = function() fired[#fired + 1] = 'tween' end})
        end)
    )");
    // clang-format on
    fixture.runLua("collectgarbage()");
    fixture.frames(4, 0.1);
    EXPECT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();
    EXPECT_EQ(fixture.lua("collectgarbage() return table.concat(fired, ',')"), "timer,tween");
}

} // namespace haylen::core
