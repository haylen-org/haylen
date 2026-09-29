#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "audio/MixerFixture.hpp"
#include "haylen/core/Engine.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::audio {

class MixerLuaTest : public MixerFixture {
  protected:
    // An engine whose Lua state holds audio, the hit sound and a one-second loop.
    [[nodiscard]] static std::unique_ptr<test::EngineFixture> makeEngine() {
        const std::vector<std::uint8_t> hit = makeWav(kRate, 1, makeSignal(4800, 1, 0.5F));
        auto fixture = std::make_unique<test::EngineFixture>(std::map<std::string, std::string>{{"content/sfx/hit.wav", std::string(hit.begin(), hit.end())}});
        fixture->runLua("audio = require('haylen.audio') events = require('haylen.events') haylen = require('haylen') hit = require('haylen.assets').load('sfx/hit.wav')");
        return fixture;
    }
};

TEST_F(MixerLuaTest, SoundsFollowTheEnginePauseByProcessMode) {
    const auto fixture = makeEngine();
    fixture->runLua("effect = audio.play(hit, {loop = true}) menu = audio.play(hit, {loop = true, processMode = 'always'})");
    EXPECT_EQ(fixture->lua("return audio.processMode(effect) .. ' ' .. audio.processMode(menu) .. ' ' .. audio.busProcessMode('sfx') .. ' ' .. audio.busProcessMode('music')"), "inherit always pausable always");

    fixture->runLua("haylen.setPaused(true)");
    // clang-format off
    const std::string stats = fixture->lua(R"(
        for _, bus in ipairs(audio.busStats()) do
            if bus.name == 'sfx' then
                return bus.voices .. ' ' .. bus.playing .. ' ' .. bus.paused .. ' ' .. tostring(bus.processing)
            end
        end
    )");
    // clang-format on
    EXPECT_EQ(stats, "2 1 1 false");

    fixture->runLua("haylen.setPaused(false) audio.setBusProcessMode('sfx', 'whenPaused')");
    EXPECT_EQ(fixture->lua("return audio.busProcessMode('sfx')"), "whenPaused");
    EXPECT_NE(fixture->lua("audio.play(hit, {processMode = 'sometimes'})").find("processMode"), std::string::npos);
    EXPECT_NE(fixture->lua("audio.setBusProcessMode('sfx', 'never')").find("error: "), std::string::npos);
}

TEST_F(MixerLuaTest, PausesEveryVoiceAndReportsInterruptions) {
    const auto fixture = makeEngine();
    fixture->runLua("mine = audio.play(hit, {loop = true}) other = audio.play(hit, {loop = true}) audio.pause(mine) audio.pauseAll() audio.resumeAll()");
    EXPECT_EQ(fixture->lua("return tostring(audio.paused(mine)) .. ' ' .. tostring(audio.paused(other))"), "true false");

    fixture->runLua("heard = {} for _, name in ipairs({'audioInterrupted', 'audioResumed', 'audioRouteChanged'}) do events.on(name, function() heard[#heard + 1] = name end) end");
    Mixer& mixer = fixture->engine().getAudio();
    mixer.reportDeviceEvent(Mixer::DeviceEvent::InterruptionBegan);
    fixture->frames(1);
    EXPECT_EQ(fixture->lua("return tostring(audio.interrupted()) .. ' ' .. table.concat(heard, ',')"), "true audioInterrupted");
    mixer.reportDeviceEvent(Mixer::DeviceEvent::InterruptionEnded);
    mixer.reportDeviceEvent(Mixer::DeviceEvent::RouteChanged);
    fixture->frames(1);
    EXPECT_EQ(fixture->lua("return tostring(audio.interrupted()) .. ' ' .. table.concat(heard, ',')"), "false audioInterrupted,audioResumed,audioRouteChanged");
}

TEST_F(MixerLuaTest, CreatesAndWiresEffects) {
    const auto fixture = makeEngine();
    // clang-format off
    fixture->runLua(R"(
        lowpass = audio.newEffect('lowpass', {cutoff = 800, q = 1})
        shelf = audio.newEffect('lowShelf', {cutoff = 200, gain = -6})
        echo = audio.newEffect('delay', {time = 0.3, maxTime = 2, feedback = 0.5, wet = 0.4, dry = 0.9})
        hall = audio.newEffect('reverb', {roomSize = 0.7, damping = 0.3, width = 0.8, wet = 0.2, dry = 1})
        plain = audio.newEffect('notch')
    )");
    // clang-format on
    EXPECT_EQ(fixture->lua("return lowpass.kind .. ' ' .. lowpass.cutoff .. ' ' .. lowpass.q .. ' ' .. lowpass.gain .. ' ' .. shelf.gain .. ' ' .. plain.kind"), "lowpass 800.0 1.0 0.0 -6.0 notch");
    EXPECT_EQ(fixture->lua("return echo.time > 0.29 and echo.maxTime == 2 and echo.feedback == 0.5 and echo.wet > 0.39 and echo.dry > 0.89 and echo.tail > 3"), "true");
    EXPECT_EQ(fixture->lua("return hall.roomSize > 0.69 and hall.damping > 0.29 and hall.width > 0.79 and hall.wet > 0.19 and hall.dry == 1 and hall.tail > 1"), "true");

    fixture->runLua("audio.addBusEffect('music', lowpass) audio.addBusEffect('music', hall) lowpass.cutoff = 400 echo.feedback = 0.25 hall.roomSize = 0.2");
    EXPECT_EQ(fixture->lua("local list = audio.busEffects('music') return #list .. ' ' .. tostring(list[1] == lowpass) .. ' ' .. tostring(list[2] == hall) .. ' ' .. tostring(list[1] == hall) .. ' ' .. lowpass.cutoff .. ' ' .. tostring(lowpass.attached)"), "2 true true false 400.0 true");
    fixture->runLua("audio.removeBusEffect('music', lowpass)");
    EXPECT_EQ(fixture->lua("return #audio.busEffects('music') .. ' ' .. tostring(lowpass.attached)"), "1 false");

    fixture->runLua("voice = audio.play(hit, {effects = {echo, shelf}})");
    EXPECT_EQ(fixture->lua("local list = audio.effects(voice) return #list .. ' ' .. tostring(list[1] == echo) .. ' ' .. tostring(list[2] == shelf)"), "2 true true");
    fixture->runLua("audio.removeEffect(voice, shelf) audio.addEffect(voice, lowpass)");
    EXPECT_EQ(fixture->lua("local list = audio.effects(voice) return #list .. ' ' .. tostring(list[2] == lowpass)"), "2 true");

    EXPECT_NE(fixture->lua("audio.newEffect('chorus')").find("Unknown audio effect: chorus"), std::string::npos);
    EXPECT_NE(fixture->lua("audio.newEffect('lowpass', {gain = 3})").find("Unknown option 'gain'"), std::string::npos);
    EXPECT_NE(fixture->lua("audio.newEffect('delay', {time = 5})").find("A delay time must be between 0"), std::string::npos);
    EXPECT_NE(fixture->lua("lowpass.gain = 2").find("Only peak and shelf filters have a gain."), std::string::npos);
    EXPECT_NE(fixture->lua("hall.width = 2").find("A reverb width must be between 0 and 1."), std::string::npos);
    EXPECT_NE(fixture->lua("audio.addBusEffect('sfx', hall)").find("already processes another bus or voice"), std::string::npos);
    EXPECT_NE(fixture->lua("audio.addBusEffect('sfx', hit)").find("audio effect expected"), std::string::npos);
    EXPECT_NE(fixture->lua("audio.play(hit, {effects = {hit}})").find("The option 'effects' must be a list of audio effects."), std::string::npos);
    EXPECT_NE(fixture->lua("audio.play(hit, {effects = 3})").find("The option 'effects' must be a list of audio effects."), std::string::npos);
    EXPECT_NE(fixture->lua("return echo.cutoff").find("haylen.Delay has no member 'cutoff'"), std::string::npos);
}

TEST_F(MixerLuaTest, TweensAnimateEffectParameters) {
    const auto fixture = makeEngine();
    fixture->runLua("tween = require('haylen.tween') lowpass = audio.newEffect('lowpass', {cutoff = 2000}) tween.to(lowpass, 0.5, {cutoff = 500})");
    fixture->frames(15);
    EXPECT_EQ(fixture->lua("return lowpass.cutoff < 2000 and lowpass.cutoff > 500"), "true");
    fixture->frames(30);
    EXPECT_EQ(fixture->lua("return lowpass.cutoff"), "500.0");
}

TEST_F(MixerLuaTest, SpatializesAndFollowsCameras) {
    const auto fixture = makeEngine();
    fixture->runLua("audio.setSpatialization({model = 'inverse', minDistance = 50, rolloff = 2, doppler = 1, speedOfSound = 1000, panDistance = 300})");
    // clang-format off
    EXPECT_EQ(fixture->lua(R"(
        local settings = audio.spatialization()
        return table.concat({settings.model, settings.minDistance, settings.maxDistance, settings.rolloff, settings.panDistance, settings.doppler, settings.speedOfSound}, ' ')
    )"), "inverse 50.0 1500.0 2.0 300.0 1.0 1000.0");
    // clang-format on
    EXPECT_NE(fixture->lua("audio.setSpatialization({model = 'cubic'})").find("model"), std::string::npos);
    EXPECT_NE(fixture->lua("audio.setSpatialization({minDistance = 0})").find("need a minimum distance above 0"), std::string::npos);
    EXPECT_NE(fixture->lua("audio.setSpatialization({range = 3})").find("Unknown option 'range'"), std::string::npos);

    fixture->runLua("camera = require('haylen.graphics2d').newCamera() camera.x = 120 camera.y = 80 audio.followCamera(camera)");
    EXPECT_EQ(fixture->lua("local x, y = audio.listener() return x .. ',' .. y"), "120.0,80.0");
    fixture->runLua("camera.x = 300");
    fixture->frames(1);
    EXPECT_EQ(fixture->lua("local x = audio.listener() return x"), "300.0");

    // The followed camera stays alive through the registry until the listener lets go of it.
    fixture->runLua("camera = nil collectgarbage() collectgarbage()");
    fixture->frames(1);
    fixture->runLua("audio.followCamera(nil) collectgarbage()");
    fixture->frames(1);
    EXPECT_EQ(fixture->lua("local x = audio.listener() return x"), "300.0");
    EXPECT_NE(fixture->lua("audio.followCamera(hit)").find("haylen.Camera expected"), std::string::npos);
}

} // namespace haylen::audio
