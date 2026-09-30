-- Interruptions and lifecycle: a track plays while every audio event, app state change and interruption of the platform shows as it happens, with the state of the output and the lifecycle options that mute or halt the app when it is not active.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Journal = require('journal')
local sample = require('sample')
local sounds = require('sounds')

local Lifecycle = haylen.class('Lifecycle', sample.Test)

local kEvents = {
    audioInterrupted = sample.red,
    audioResumed = sample.green,
    audioRouteChanged = sample.warm,
    appActive = sample.accent,
    appInactive = sample.violet,
    appBackground = sample.violet,
}
local kOrder = {'audioInterrupted', 'audioResumed', 'audioRouteChanged', 'appActive', 'appInactive', 'appBackground'}
local kNotes = {
    'Background: the engine stops the output and every voice keeps its place. The output starts again when the app comes back, unless an interruption still holds it.',
    'Interruption: a phone call, an alarm, Siri or another Android app with the audio focus pauses every voice and sends "audioInterrupted". The event "audioResumed" follows once it ends and the app is active again.',
    'Route change: unplugging headphones sends "audioRouteChanged", where a music player would pause.',
    'Web: the browser keeps the sound silent until the first click, tap or key on the page, and a hidden tab goes to the background.',
}

function Lifecycle:enter()
    self.saved = haylen.lifecycle()
    self.clock = 0
    self.counts = {}
    self.journal = Journal(26)
    for _, name in ipairs(kOrder) do
        self.counts[name] = 0
        self:listen(name, function()
            self.counts[name] = self.counts[name] + 1
            self.journal:add(string.format('%s at %.1fs, interrupted %s, state %s', name, self.clock, audio.interrupted(), haylen.appState()), kEvents[name])
        end)
    end
    audio.playMusic(sounds.track(sounds.tracks[1].path), {fade = 1, volume = 0.7})

    local controls = {
        ui.button{id = 'click', text = 'Play a click', variant = 'primary', onClick = function() audio.play(sounds.get('audio/ui/confirm.ogg'), {bus = 'ui'}) end},
        ui.toggle{id = 'mute', text = 'Mute while not active', checked = self.saved.muteOnFocusLoss, onChange = function(event) haylen.setLifecycle({muteOnFocusLoss = event.checked}) end},
        ui.toggle{id = 'halt', text = 'Halt while not active', checked = self.saved.pauseOnFocusLoss, onChange = function(event) haylen.setLifecycle({pauseOnFocusLoss = event.checked}) end},
        ui.toggle{id = 'background', text = 'Halt in the background', checked = self.saved.pauseOnBackground, onChange = function(event) haylen.setLifecycle({pauseOnBackground = event.checked}) end},
    }
    for _, note in ipairs(kNotes) do
        controls[#controls + 1] = ui.label{text = note, color = 'textMuted', font = 'caption'}
    end
    self:frame({hint = 'Switch to another app or tab, take a call or unplug headphones while the music plays.', focus = 'click', controls = controls})
end

function Lifecycle:exit()
    haylen.setLifecycle(self.saved)
    audio.stopMusic(0.3)
end

-- The platform reports interruptions as events of the scene too, before the engine turns them into audio events.
function Lifecycle:event(event)
    if event.type == 'interruptionBegan' or event.type == 'interruptionEnded' or event.type == 'suspended' or event.type == 'resumed' then
        self.journal:add('platform event ' .. event.type, sample.muted)
    end
end

function Lifecycle:update(dt)
    Lifecycle.super.update(self, dt)
    self.clock = self.clock + haylen.unscaledDelta()
    self:status(string.format('app %s   halted %s   interrupted %s   device %s   output %s   %d Hz, %d channels   voices %d', haylen.appState(), haylen.halted(), audio.interrupted(), audio.hasDevice(), audio.outputAvailable() and 'available' or 'unavailable, no sound', audio.sampleRate(), audio.channels(), audio.voiceCount()))
end

function Lifecycle:draw(area)
    local interrupted = audio.interrupted()
    graphics2d.drawRect({24, 24, 360, 120}, interrupted and '#FF4A2630' or '#FF1F3A2E')
    sample.caption(interrupted and 'Interrupted' or 'Audio running', 44, 44, {size = 34, color = interrupted and sample.red or sample.green})
    sample.caption('audio.interrupted() ' .. tostring(interrupted), 44, 96, {size = 20})

    local left = 420
    for index, name in ipairs(kOrder) do
        local x = left + ((index - 1) % 3) * 290
        local y = 24 + ((index - 1) // 3) * 64
        graphics2d.drawRect({x, y, 274, 54}, sample.surface)
        sample.caption(name, x + 12, y + 8, {size = 18, color = kEvents[name]})
        sample.caption(tostring(self.counts[name]), x + 262, y + 27, {size = 26, color = sample.ink, anchor = {1, 0.5}})
    end

    sample.caption('Events as they happen', 24, 180)
    self.journal:draw(24, 216, area.height - 240, 20)
end

return Lifecycle
