-- Voice limits: a hundred coins collected at once with and without the limit of the coin sound, a line of dialogue whose priority keeps it playing while bursts fill every voice, and a tone that stops and resumes in a fast loop without clicks.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local AudioTest = require('categories.audio.audio-test')
local sounds = require('categories.audio.sounds')
local Test = require('harness.test')

local VoiceLimits = haylen.class('VoiceLimits', AudioTest)

local kCoins = 100
local kCode = "audio.setLimit(coin, {voices = 4, cooldown = 0.03})\naudio.play(line, {priority = 255})\naudio.pause(voice) audio.resume(voice)"

function VoiceLimits:enter()
    self.coin = sounds.get('audio/effects/wood-pickup.ogg')
    self.burst = sounds.get('audio/effects/footstep-1.ogg')
    self.line = sounds.get('audio/generated/pluck-loop.wav')
    self.tone = sounds.get('audio/generated/hum-loop.wav')
    self.limited, self.flicker, self.flickerTime = true, false, 0
    self.peakVoices, self.lineVoice = 0, nil
    self:applyLimit()
    self:frame({
        code = kCode,
        hint = 'Collect a hundred coins with and without the limit and compare the voices, fill every voice while the line plays, and let the tone stop and resume quickly.',
        controls = {
            ui.toggle{id = 'limit', text = 'Limit the coin sound to four voices', checked = true, onChange = function(event)
                self.limited = event.checked
                self:applyLimit()
            end},
            ui.button{id = 'coins', text = 'Collect a hundred coins', variant = 'primary', onClick = function() self:collect() end},
            ui.button{id = 'line', text = 'Play a line with priority 255', onClick = function() self:playLine() end},
            ui.button{id = 'burst', text = 'Fill every voice', onClick = function() self:fill() end},
            ui.toggle{id = 'flicker', text = 'Stop and resume a tone every 50 milliseconds', onChange = function(event) self:setFlicker(event.checked) end},
        },
    })
end

function VoiceLimits:exit()
    VoiceLimits.super.exit(self)
    self:setFlicker(false)
    audio.setLimit(self.coin, {})
    audio.stopAll(0.1)
end

function VoiceLimits:applyLimit()
    audio.setLimit(self.coin, self.limited and {voices = 4, cooldown = 0.03} or {})
end

function VoiceLimits:collect()
    for _ = 1, kCoins do
        audio.play(self.coin, {pitchVariation = 0.1, volume = 0.5})
    end
end

function VoiceLimits:playLine()
    if self.lineVoice then
        audio.stop(self.lineVoice)
    end
    self.lineVoice = audio.play(self.line, {priority = 255, loop = true, volume = 0.6})
end

function VoiceLimits:fill()
    for _ = 1, 140 do
        audio.play(self.burst, {volume = 0.2, priority = 64})
    end
end

function VoiceLimits:setFlicker(enabled)
    self.flicker = enabled
    if enabled and not self.toneVoice then
        self.toneVoice = audio.play(self.tone, {loop = true, volume = 0.5})
    elseif not enabled and self.toneVoice then
        audio.stop(self.toneVoice)
        self.toneVoice = nil
    end
end

function VoiceLimits:update(dt)
    VoiceLimits.super.update(self, dt)
    if self.flicker and self.toneVoice then
        self.flickerTime = self.flickerTime + dt
        if self.flickerTime >= 0.05 then
            self.flickerTime = 0
            if audio.paused(self.toneVoice) then
                audio.resume(self.toneVoice)
            else
                audio.pause(self.toneVoice)
            end
        end
    end
    self.peakVoices = math.max(self.peakVoices * 0.98, audio.voiceCount())
    local line = self.lineVoice and audio.active(self.lineVoice) and 'playing' or 'not playing'
    self:status(string.format('Voices %d, line %s, coin limit %s', audio.voiceCount(), line, self.limited and 'on' or 'off'))
end

function VoiceLimits:draw(area)
    Test.caption('Voices that play now', 40, 40, {size = 24, color = Test.ink})
    AudioTest.bar(40, 80, area.width - 80, 28, audio.voiceCount() / 128, Test.accent)
    AudioTest.bar(40, 120, area.width - 80, 12, self.peakVoices / 128, Test.warm)
    Test.caption(string.format('%d of 128 voices, recent peak %d', audio.voiceCount(), math.floor(self.peakVoices)), 40, 140)
    local playing = self.lineVoice and audio.active(self.lineVoice)
    graphics2d.drawCircle(80, 240, 30, playing and Test.green or AudioTest.idle, {layer = 1})
    Test.caption(playing and 'The line keeps playing with priority 255' or 'The line is silent', 130, 228, {color = Test.ink})
end

return VoiceLimits
