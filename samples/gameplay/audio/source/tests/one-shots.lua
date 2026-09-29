-- One-shot effects: a sound played with a volume, a pitch with a random variation, a pan, a fade in and a fade out, a limit of voices per sound that stops the oldest one, and every voice drawn with the pitch it got, its pan and its cursor.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local sample = require('sample')
local sounds = require('sounds')

local OneShots = haylen.class('OneShots', sample.Test)

local kCode = "audio.play(sound, {volume = 0.8, pitch = 1, pitchVariation = 0.1,\n  pan = -0.5, fadeIn = 0.2, loop = false})\naudio.stop(voice, 0.3)\naudio.seedVariation(7)"

function OneShots:enter()
    self.sound = sounds.effects[1]
    self.options = {volume = 1, pitch = 1, pitchVariation = 0.1, pan = 0, fadeIn = 0}
    self.fadeOut, self.limit, self.loop = 0.2, 4, false
    self.voices = {}
    local function slider(id, label, min, max, step)
        return ui.formField{label = label, ui.slider{id = id, min = min, max = max, step = step, value = self.options[id] or self[id], showValue = true, onChange = function(event)
            if self.options[id] then
                self.options[id] = event.value
            else
                self[id] = event.value
            end
        end}}
    end
    self:frame({
        hint = 'Click the stage to play at a pan that follows the pointer.',
        focus = 'play',
        controls = {
            ui.formField{label = 'Sound', ui.combo{id = 'sound', items = sounds.items(sounds.effects), selected = self.sound.id, onChange = function(event) self.sound = sounds.find(sounds.effects, event.value) end}},
            ui.row{gap = 12,
                ui.button{id = 'play', text = 'Play', variant = 'primary', onClick = function() self:play(self.options.pan) end},
                ui.button{id = 'stopAll', text = 'Stop every voice', onClick = function() self:stopAll() end},
            },
            slider('volume', 'Volume', 0, 1.5, 0.05),
            slider('pitch', 'Pitch', 0.5, 2, 0.05),
            slider('pitchVariation', 'Pitch variation', 0, 0.45, 0.01),
            slider('pan', 'Pan', -1, 1, 0.05),
            slider('fadeIn', 'Fade in seconds', 0, 1, 0.05),
            slider('fadeOut', 'Fade out seconds when stopped', 0, 1, 0.05),
            ui.formField{label = 'Voices of one sound at once', ui.stepper{id = 'limit', min = 1, max = 8, value = self.limit, onChange = function(event) self.limit = event.value end}},
            ui.toggle{id = 'loop', text = 'Loop, to hear the fades', onChange = function(event) self.loop = event.checked end},
            ui.button{id = 'seed', text = 'Seed the variation with 7', onClick = function() audio.seedVariation(7) end},
            ui.label{text = kCode, font = 'monospace'},
        },
    })
end

function OneShots:exit()
    self:stopAll()
end

function OneShots:stopAll()
    for _, voice in ipairs(self.voices) do
        audio.stop(voice.id, self.fadeOut)
    end
end

-- Plays the chosen sound, first stopping the oldest voices of the same sound past the limit.
function OneShots:play(pan)
    local same = {}
    for _, voice in ipairs(self.voices) do
        if voice.sound == self.sound and audio.active(voice.id) then
            same[#same + 1] = voice
        end
    end
    for index = 1, #same - self.limit + 1 do
        audio.stop(same[index].id, 0.05)
        same[index].limited = true
    end

    local options = {volume = self.options.volume, pitch = self.options.pitch, pan = pan, fadeIn = self.options.fadeIn, loop = self.loop}
    -- The variation must stay below the pitch, so a low pitch narrows it.
    options.pitchVariation = math.min(self.options.pitchVariation, self.options.pitch - 0.01)
    local sound = sounds.get(self.sound.path)
    self.voices[#self.voices + 1] = {id = audio.play(sound, options), sound = self.sound, duration = sound.duration, pan = pan}
end

function OneShots:update(dt)
    OneShots.super.update(self, dt)
    if not self.area then
        return
    end
    for index = #self.voices, 1, -1 do
        if not audio.active(self.voices[index].id) then
            table.remove(self.voices, index)
        end
    end
    if input.mousePressed('left') and not ui.usingPointer() then
        local x = self:toStage(input.mousePosition())
        if x >= 0 and x <= self.area.width then
            self:play(math.max(-1, math.min(1, x / self.area.width * 2 - 1)))
        end
    end
    local last = self.voices[#self.voices]
    self:status(string.format('voices of the sample %d   engine voices %d   last pitch %.3f', #self.voices, audio.voiceCount(), last and audio.pitch(last.id) or 0))
end

function OneShots:draw(area)
    local left, width = 40, area.width - 80
    local panLeft, panRight = left + 420 + width * 0.4, left + width - 20
    sample.caption('Every voice: the pitch it got, its playback cursor and its pan', left, 20)
    for index, voice in ipairs(self.voices) do
        local y = 60 + (index - 1) * 48
        if y > area.height - 80 then
            break
        end
        local progress = voice.duration > 0 and (audio.cursor(voice.id) % voice.duration) / voice.duration or 0
        local color = voice.limited and sample.red or sample.accent
        sample.caption(string.format('%d  %s  pitch %.3f', voice.id, voice.sound.text, audio.pitch(voice.id)), left, y, {color = voice.limited and sample.red or sample.ink, size = 20})
        sample.bar(left + 380, y + 4, width * 0.4, 16, progress, color)
        graphics2d.drawLine(panLeft, y + 12, panRight, y + 12, 2, sample.line, {layer = 1})
        graphics2d.drawCircle(panLeft + (panRight - panLeft) * (voice.pan + 1) / 2, y + 12, 9, sample.green, {layer = 2})
    end
    sample.caption('left', panLeft, area.height - 40, {size = 18})
    sample.caption('right', panRight, area.height - 40, {size = 18, anchor = {1, 0}})
    sample.caption('Red voices were stopped by the limit', left, area.height - 40, {size = 18, color = sample.red})
end

return OneShots
