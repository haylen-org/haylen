-- Interruptions and lifecycle: a track plays while every audio event, app state change and interruption of the platform shows as it happens, with the state of the output and the lifecycle options that mute or halt the app when it is not active.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local AudioTest = require('categories.audio.audio-test')
local EventLog = require('categories.audio.event-log')
local sounds = require('categories.audio.sounds')
local Test = require('harness.test')

local Lifecycle = haylen.class('Lifecycle', AudioTest)

Lifecycle.code = [[
haylen.setLifecycle({muteOnFocusLoss = true, pauseOnFocusLoss = false, pauseOnBackground = true})
events.on('audioInterrupted', function() showPaused() end, {owner = self})  -- Every voice paused.
events.on('audioResumed', function() hidePaused() end, {owner = self})
events.on('audioRouteChanged', function() audio.pauseAll() end, {owner = self})  -- Headphones were unplugged.
print(haylen.appState(), audio.interrupted(), audio.outputAvailable())]]

local kEvents = {
    audioInterrupted = Test.red,
    audioResumed = Test.green,
    audioRouteChanged = Test.warm,
    appActive = Test.accent,
    appInactive = Test.violet,
    appBackground = Test.violet,
}
local kOrder = {'audioInterrupted', 'audioResumed', 'audioRouteChanged', 'appActive', 'appInactive', 'appBackground'}
local kNotes = {
    'Background: the engine stops the output and every voice keeps its place. The output starts again when the app comes back, unless an interruption still holds it.',
    'Interruption: a phone call, an alarm, a voice assistant or another app that takes the audio focus pauses every voice and sends "audioInterrupted". The event "audioResumed" follows once it ends and the app is active again.',
    'Route change: unplugging headphones sends "audioRouteChanged", where a music player would pause.',
    'Web: the browser keeps the sound silent until the first click, tap or key on the page, and a hidden tab goes to the background.',
}

function Lifecycle:enter()
    self.saved = haylen.lifecycle()
    self.clock = 0
    self.counts = {}
    self.log = EventLog({
        title = 'Events as they happen',
        hint = 'Next: switch to another app or tab, take a call or unplug headphones while the music plays. Each event shows here with the color of its counter.',
        legend = {{Test.ink, 'This test'}, {Test.muted, 'The platform'}},
    })
    for _, name in ipairs(kOrder) do
        self.counts[name] = 0
        self:listen(name, function()
            self.counts[name] = self.counts[name] + 1
            self.log:add(self.clock, string.format('The event "%s" arrived %s, in the app state "%s".', name, audio.interrupted() and 'while an interruption holds the sound' or 'with the sound free', haylen.appState()), kEvents[name])
        end)
    end
    self:listen('audioUnlocked', function() self.log:add(self.clock, 'The browser let the sound play after the first click, tap or key on the page.', Test.muted) end)
    audio.playMusic(sounds.track(sounds.tracks[1].path), {fade = 1, volume = 0.7})
    self.log:add(0, string.format('The test started the calm loop in the app state "%s".', haylen.appState()), Test.ink)
    if AudioTest.locked() then
        self.log:add(0, 'The browser holds the sound until the first click, tap or key on the page.', Test.muted)
    end

    local controls = {
        ui.button{id = 'chime', text = 'Play a chime', variant = 'primary', onClick = function() audio.play(sounds.get('audio/ui/confirm.ogg'), {bus = 'ui'}) end},
        ui.toggle{id = 'mute', text = 'Mute while not active', checked = self.saved.muteOnFocusLoss, onChange = function(event) haylen.setLifecycle({muteOnFocusLoss = event.checked}) end},
        ui.toggle{id = 'halt', text = 'Halt while not active', checked = self.saved.pauseOnFocusLoss, onChange = function(event) haylen.setLifecycle({pauseOnFocusLoss = event.checked}) end},
        ui.toggle{id = 'background', text = 'Halt in the background', checked = self.saved.pauseOnBackground, onChange = function(event) haylen.setLifecycle({pauseOnBackground = event.checked}) end},
    }
    for _, note in ipairs(kNotes) do
        controls[#controls + 1] = ui.label{text = note, color = 'textMuted', font = 'caption'}
    end
    self:frame({hint = 'Switch to another app or tab, take a call or unplug headphones while the music plays.', code = Lifecycle.code, focus = 'chime', controls = controls})
end

function Lifecycle:exit()
    Lifecycle.super.exit(self)
    haylen.setLifecycle(self.saved)
    audio.stopMusic(0.3)
end

-- The platform reports interruptions as events of the scene too, before the engine turns them into audio events.
function Lifecycle:event(event)
    if event.type == 'interruptionBegan' or event.type == 'interruptionEnded' or event.type == 'suspended' or event.type == 'resumed' then
        self.log:add(self.clock, 'The platform sent "' .. event.type .. '".', Test.muted)
    end
end

function Lifecycle:update(dt)
    Lifecycle.super.update(self, dt)
    self.clock = self.clock + haylen.unscaledDelta()
    self:status(string.format('The app is "%s"%s. %s', haylen.appState(), haylen.halted() and ' and halted, so its frames and its sound wait' or '', Lifecycle.sound()))
end

-- Says in a sentence what the output does: locked by a browser, held by an interruption, without a device or an output, or playing.
function Lifecycle.sound()
    if AudioTest.locked() then
        return 'The browser holds the sound until the first click, tap or key on the page.'
    elseif audio.interrupted() then
        return 'An interruption holds the sound, and every voice waits for it to end.'
    elseif not audio.hasDevice() then
        return 'No audio device is open, so the voices play in silence.'
    elseif not audio.outputAvailable() then
        return 'The device offers no output now, so the voices play in silence.'
    end
    local channels = audio.channels() == 2 and 'stereo' or audio.channels() == 1 and 'mono' or audio.channels() .. ' channels'
    return string.format('The device plays %s at %d Hz in %s.', AudioTest.count(audio.voiceCount(), 'voice', 'voices'), audio.sampleRate(), channels)
end

function Lifecycle:draw(area)
    -- The card of the state turns red while an interruption holds the sound, and shows a padlock while a browser keeps it locked.
    local interrupted, locked = audio.interrupted(), AudioTest.locked()
    local state = interrupted and Test.red or locked and Test.warm or Test.green
    graphics2d.drawShape({24, 24, 360, 132}, {radius = 18, color = interrupted and '#FF3A2430' or locked and '#FF2A2618' or '#FF1D3329', borderWidth = 2, borderColor = state})
    if locked then
        AudioTest.padlock(56, 62, 30, state, '#FF2A2618', 1)
    else
        graphics2d.drawCircle(56, 64, 10, state, {layer = 1})
    end
    Test.caption(interrupted and 'Interrupted' or locked and 'Sound locked' or 'Audio running', 82, 44, {size = 32, color = state, layer = 1})
    Test.caption('The call "audio.interrupted()" returns "' .. tostring(interrupted) .. '".', 44, 100, {size = 19, maxWidth = 320, layer = 1})

    -- The counters fill the width beside the state, three to a row, each with its count under its name.
    local left, gap = 408, 16
    local width = (area.width - left - 24 - gap * 2) / 3
    for index, name in ipairs(kOrder) do
        local x = left + ((index - 1) % 3) * (width + gap)
        local y = 24 + ((index - 1) // 3) * 66
        graphics2d.drawShape({x, y, width, 58}, {radius = 14, color = Test.surface, borderWidth = 1, borderColor = Test.line})
        graphics2d.drawCircle(x + 20, y + 29, 6, kEvents[name], {layer = 1})
        Test.caption('"' .. name .. '"', x + 36, y + 17, {size = 19, color = Test.ink, maxWidth = width - 100, layer = 1})
        Test.caption(tostring(self.counts[name]), x + width - 18, y + 29, {size = 28, color = Test.ink, anchor = {1, 0.5}, layer = 1})
    end

    self.log:draw(24, 180, area.width - 48, area.height - 204)
end

return Lifecycle
