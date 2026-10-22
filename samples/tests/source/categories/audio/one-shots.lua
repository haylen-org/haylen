-- One-shot effects: a sound played with a volume, a pitch with a random variation, a pan, a fade in and a fade out, the limit of voices of each sound that stops the oldest one, and every voice drawn at its pan with its cursor and the pitch it got, on a field where a click plays at the pan of that spot.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local timer = require('haylen.timer')
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
    -- The test opens with three voices from the left speaker to the right one.
    for index, pan in ipairs({-0.7, 0, 0.7}) do
        timer.after(0.3 * index, function() self:play(pan) end, {owner = self})
    end
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
    if self.pointer.pressed and self.field then
        self:play(self:panAt(self.pointer.worldX))
    end
    local playing = #self.voices == 0 and 'No voice of the test plays' or AudioTest.count(#self.voices, 'voice', 'voices') .. ' of the test play'
    local stopped = self.stolen == 0 and 'has stopped none yet' or 'has stopped ' .. AudioTest.count(self.stolen, 'voice', 'voices')
    self:status(string.format('%s, and the limit of %s of one sound %s.', playing, AudioTest.count(self.limit, 'voice', 'voices'), stopped))
end

function OneShots:renderUi()
    self.pointer:draw()
end

-- The field spans the stage from the left speaker to the right one, so the pan of a voice and the pan of a click share one scale.
function OneShots:panAt(x)
    local field = self.field
    return math.max(-1, math.min(1, (x - field.left) / (field.right - field.left) * 2 - 1))
end

function OneShots:panX(pan)
    return self.field.left + (self.field.right - self.field.left) * (pan + 1) / 2
end

-- Draws a speaker at `x`, `y` whose waves point to the right for a `direction` of 1 and to the left for -1, at each end of the field.
function OneShots.speaker(x, y, direction)
    local d = direction
    graphics2d.drawShape({x - 8 * d - 5, y - 8, 10, 16}, {radius = 2, color = Test.muted, layer = 1})
    graphics2d.drawPolygon({{x - 3 * d, y - 8}, {x + 8 * d, y - 16}, {x + 8 * d, y + 16}, {x - 3 * d, y + 8}}, Test.muted, {layer = 1})
    local from = d > 0 and -math.pi / 4 or math.pi * 0.75
    for _, radius in ipairs({11, 19}) do
        graphics2d.drawArc(x + 8 * d, y, radius, 3, from, from + math.pi / 2, Test.muted, {layer = 1})
    end
end

-- Every voice is a marker at its pan, with a ring of its playback cursor and the pitch it got beside it, on the field that a click plays on.
function OneShots:draw(area)
    local left, width = 40, area.width - 80
    local top, bottom = 72, area.height - 24
    self.field = {left = left + 70, right = left + width - 70}
    Test.caption('Every voice at its pan, with its playback and the pitch it got', left, 20, {size = 22, color = Test.ink})
    AudioTest.lockNote(left + width, 10)
    graphics2d.drawShape({left, top, width, bottom - top}, {radius = 16, color = Test.surface, borderWidth = 1, borderColor = Test.line})

    for index, name in ipairs({'Left', 'Half left', 'Center', 'Half right', 'Right'}) do
        local x = self:panX((index - 3) / 2)
        graphics2d.drawLine(x, top + 52, x, bottom - 56, index == 3 and 2 or 1, Test.line, {layer = 1})
        Test.caption(name, x, top + 16, {size = 19, anchor = {0.5, 0}, color = index % 2 == 1 and Test.ink or Test.muted})
    end
    Test.caption('Click or tap anywhere on the field to play the sound at the pan of that spot.', left + width / 2, bottom - 40, {size = 19, anchor = {0.5, 0}})
    OneShots.speaker(left + 32, (top + bottom) / 2, 1)
    OneShots.speaker(left + width - 32, (top + bottom) / 2, -1)

    local rowTop = top + 84
    for index, voice in ipairs(self.voices) do
        local y = rowTop + (index - 1) * 54
        if y > bottom - 80 then
            break
        end
        local x = self:panX(voice.pan)
        local progress = voice.duration > 0 and (audio.cursor(voice.id) % voice.duration) / voice.duration or 0
        graphics2d.drawRing(x, y, 21, 5, AudioTest.idle, {layer = 2})
        if progress > 0 then
            graphics2d.drawArc(x, y, 21, 5, -math.pi / 2, -math.pi / 2 + progress * math.pi * 2, Test.accent, {layer = 3})
        end
        graphics2d.drawCircle(x, y, 12, Test.green, {layer = 3})
        local right = voice.pan > 0.25
        Test.caption(string.format('Voice %d, %s at a pitch of %.3f', voice.id, voice.sound.text:lower(), audio.pitch(voice.id)), x + (right and -36 or 36), y, {size = 20, color = Test.ink, anchor = {right and 1 or 0, 0.5}, layer = 3})
    end
end

return OneShots
