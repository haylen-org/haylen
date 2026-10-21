-- One-shot effects: a sound played with a volume, a pitch with a random variation, a pan, a fade in and a fade out, the limit of voices of each sound that stops the oldest one, and every voice drawn with the pitch it got, its pan and its cursor.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local AudioTest = require('categories.audio.audio-test')
local Pointer = require('harness.pointer')
local sounds = require('categories.audio.sounds')
local Test = require('harness.test')

local OneShots = haylen.class('OneShots', AudioTest)

local kCode = [[
audio.setLimit(sound, {voices = 4})  -- A fifth voice stops the oldest one.
local voice = audio.play(sound, {volume = 0.8, pitch = 1, pitchVariation = 0.1, pan = -0.5, fadeIn = 0.2})
audio.stop(voice, 0.3)  -- Fades out over 0.3 seconds.
audio.seedVariation(7)  -- The variations repeat from here.]]

function OneShots:enter()
    self.sound = sounds.effects[1]
    self.options = {volume = 1, pitch = 1, pitchVariation = 0.1, pan = 0, fadeIn = 0}
    self.fadeOut, self.limit, self.loop, self.stolen = 0.2, 4, false, 0
    self.voices = {}
    self.pointer = Pointer()
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
        code = kCode,
        hint = 'Click or tap the stage, or aim with the right stick and press the right trigger, to play at the pan of that spot.',
        focus = 'play',
        controls = {
            ui.formField{label = 'Sound', ui.combo{id = 'sound', items = sounds.items(sounds.effects), selected = self.sound.id, onChange = function(event) self.sound = sounds.find(sounds.effects, event.value) end}},
            ui.row{wrap = true, gap = 12,
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
        },
    })
    self:loadActions({actions = Pointer.actions})
end

function OneShots:exit()
    OneShots.super.exit(self)
    self:stopAll()
    for _, effect in ipairs(sounds.effects) do
        audio.setLimit(sounds.get(effect.path), {})
    end
end

function OneShots:stopAll()
    for _, voice in ipairs(self.voices) do
        audio.stop(voice.id, self.fadeOut)
    end
end

-- Plays the chosen sound under the limit of its voices, which stops its oldest voice when every one of them plays.
function OneShots:play(pan)
    local options = {volume = self.options.volume, pitch = self.options.pitch, pan = pan, fadeIn = self.options.fadeIn, loop = self.loop}
    -- The variation must stay below the pitch, so a low pitch narrows it.
    options.pitchVariation = math.min(self.options.pitchVariation, self.options.pitch - 0.01)
    local sound = sounds.get(self.sound.path)
    audio.setLimit(sound, {voices = self.limit})
    local playing = {}
    for _, voice in ipairs(self.voices) do
        if voice.sound == self.sound and audio.active(voice.id) then
            playing[#playing + 1] = voice
        end
    end
    self.voices[#self.voices + 1] = {id = audio.play(sound, options), sound = self.sound, duration = sound.duration, pan = pan}
    for _, voice in ipairs(playing) do
        if not audio.active(voice.id) then
            self.stolen = self.stolen + 1
        end
    end
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
    self.pointer:update(dt, self)
    if self.pointer.pressed then
        self:play(math.max(-1, math.min(1, self.pointer.worldX / self.area.width * 2 - 1)))
    end
    local last = self.voices[#self.voices]
    self:status(string.format('%d voices of the test, %d in the engine, %d stopped by the limit, the last at a pitch of %.3f', #self.voices, audio.voiceCount(), self.stolen, last and audio.pitch(last.id) or 0))
end

function OneShots:renderUi()
    self.pointer:draw()
end

-- Every voice is a row with the pitch it got, a bar of its cursor and a dot at its pan, over the pan field that a click plays on.
function OneShots:draw(area)
    local left, width = 40, area.width - 80
    local panLeft, panRight = left + width * 0.58, left + width - 16
    local axis = area.height - 70
    Test.caption('Every voice: the pitch it got, its playback cursor and its pan', left, 20, {size = 22, color = Test.ink})
    if #self.voices == 0 then
        Test.caption('No voice plays. Click or tap anywhere on the stage to play the sound at the pan of that spot, from the left to the right.', left, 64, {size = 21, maxWidth = width})
    end
    for index, voice in ipairs(self.voices) do
        local y = 64 + (index - 1) * 44
        if y > axis - 60 then
            break
        end
        local progress = voice.duration > 0 and (audio.cursor(voice.id) % voice.duration) / voice.duration or 0
        Test.caption(string.format('Voice %d, %s, pitch %.3f', voice.id, voice.sound.text:lower(), audio.pitch(voice.id)), left, y, {color = Test.ink, size = 20})
        AudioTest.bar(left + 330, y + 5, panLeft - left - 370, 14, progress, Test.accent)
        graphics2d.drawLine(panLeft, y + 12, panRight, y + 12, 2, Test.line, {layer = 1})
        graphics2d.drawShape({panLeft + (panRight - panLeft) * (voice.pan + 1) / 2 - 9, y + 3, 18, 18}, {kind = 'ellipse', color = Test.green, borderWidth = 2, borderColor = '#FF15172A', layer = 2})
    end

    -- The pan field runs along the bottom of the stage, from the left speaker to the right one.
    graphics2d.drawShape({left, axis, width, 12}, {radius = 6, color = Test.surface, borderWidth = 1, borderColor = Test.line})
    for index = 0, 4 do
        local x = left + width * index / 4
        graphics2d.drawLine(x, axis - 6, x, axis + 18, 2, Test.line, {layer = 1})
    end
    Test.caption('Left', left, axis + 24, {size = 19})
    Test.caption('Center', left + width / 2, axis + 24, {size = 19, anchor = {0.5, 0}})
    Test.caption('Right', left + width, axis + 24, {size = 19, anchor = {1, 0}})
end

return OneShots
