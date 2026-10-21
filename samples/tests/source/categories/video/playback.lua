-- The function `video.load` opens a video file of the package with the decoder of the platform, and the texture of the video draws like any other: as a framed sprite on the stage, as a smaller sprite that turns on a card of its own, and as an image of the UI that `ui.setImage` names. The sound plays on the music bus.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local video = require('haylen.video')

local Test = require('harness.test')

local Playback = haylen.class('Playback', Test)

Playback.code = [[
self.clip = video.load('video/clip.mp4', {autoplay = true, loop = true, owner = self})
self.clip:on('ready', function()
    ui.setImage('@clip', self.clip.texture)  -- The video as an image of the UI.
end, {owner = self})
graphics2d.draw(self.clip.texture, x, y, {width = width, height = height})
self.clip:seek(0)]]

Playback.states = {loading = 'loading', ready = 'ready', playing = 'playing', paused = 'paused', ended = 'ended', error = 'failed'}

function Playback:enter()
    self.events = {}
    self.font = assets.font('fonts/lilita-one-regular.ttf')
    self.clip = video.load('video/clip.mp4', {autoplay = true, loop = true, owner = self})
    self.clip:on('ready', function()
        self:note(string.format('Ready: %d by %d pixels, %.2f seconds.', self.clip.width, self.clip.height, self.clip.duration))
        ui.setImage('@clip', self.clip.texture)
        self:set('picture', {visible = true})
    end, {owner = self})
    self.clip:on('ended', function() self:note('The clip ended.') end, {owner = self})
    self.clip:on('error', function(error) self:note('The clip failed with the code "' .. error.code .. '": ' .. error.message) end, {owner = self})
    self:frame{
        code = Playback.code,
        hint = 'The hero runs over the island, and a note rises every second.',
        controls = {
            ui.row{wrap = true, gap = 8,
                ui.button{text = 'Play', variant = 'primary', onClick = function() self.clip:play() end},
                ui.button{text = 'Pause', onClick = function() self.clip:pause() end},
                ui.button{text = 'From the start', onClick = function() self.clip:seek(0) end},
            },
            ui.toggle{text = 'Loop', checked = true, onChange = function(event) self.clip.loop = event.checked end},
            ui.formField{label = 'Rate', ui.segmentedControl{selected = '1', items = {{id = '0.5', text = '0.5x'}, {id = '1', text = '1x'}, {id = '2', text = '2x'}}, onChange = function(event) self.clip.rate = tonumber(event.value) end}},
            ui.formField{label = 'Volume', ui.slider{value = 1, showValue = true, onChange = function(event) self.clip.volume = event.value end}},
            ui.toggle{text = 'Muted', onChange = function(event) self.clip.muted = event.checked end},
            ui.formField{label = 'As an image of the UI', ui.image{id = 'picture', image = '@clip', width = 320, height = 180, radius = 12, visible = false}},
            ui.formField{label = 'Events', ui.label{id = 'events', text = 'Loading the clip.', color = 'textMuted'}},
        },
        panelWidth = 520,
    }
end

function Playback:note(line)
    table.insert(self.events, 1, line)
    self.events[4] = nil
    self:set('events', {text = table.concat(self.events, '\n')})
end

function Playback:exit()
    ui.removeImage('@clip')
    Playback.super.exit(self)
end

function Playback:update(dt)
    Playback.super.update(self, dt)
    local clip = self.clip
    self:status(string.format('The clip is %s at %.2f of %.2f seconds, frame %d, at %gx.', clip.playing and 'playing' or (Playback.states[clip.state] or clip.state), clip.time, clip.duration, clip.frameCount, clip.rate))
end

-- Draws the clip as a framed screen with a shadow, its time as a bar under it, and a smaller copy that turns on a card in the corner.
function Playback:draw(area)
    local width = math.min(area.width * 0.62, (area.height - 140) * 16 / 9, 900)
    local height = width * 9 / 16
    local x, y = area.width * 0.42, area.height * 0.44
    local screen = {x - width / 2, y - height / 2, width, height}
    graphics2d.drawShape({screen[1] - 14, screen[2] - 4, width + 28, height + 28}, {radius = 26, color = '#80000000', softness = 24})
    graphics2d.drawShape({screen[1] - 12, screen[2] - 12, width + 24, height + 24}, {radius = 24, color = '#FF2F3352', borderWidth = 3, borderColor = '#FF15172A'})
    graphics2d.drawShape(screen, {radius = 14, texture = self.clip.texture, layer = 1})

    local bar = {screen[1], screen[2] + height + 32, width, 16}
    graphics2d.drawShape(bar, {radius = 8, color = '#FF15172A', layer = 1})
    local share = self.clip.duration > 0 and self.clip.time / self.clip.duration or 0
    graphics2d.drawShape({bar[1] + 3, bar[2] + 3, math.max(10, (width - 6) * share), 10}, {radius = 5, color = '#FFFFC93C', layer = 2})
    graphics2d.drawText(nil, 'A sprite drawn from the texture of the video, with its time under it', x, bar[2] + 34, {size = 22, color = Test.muted, anchor = {0.5, 0}, layer = 2})

    local card = {area.width - 300, area.height - 280, 276, 256}
    graphics2d.drawShape(card, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    graphics2d.draw(self.clip.texture, card[1] + card[3] / 2, card[2] + 110, {width = 160, height = 90, rotation = haylen.elapsed() * 0.5, layer = 1})
    graphics2d.drawText(self.font, 'The same texture, turning', card[1] + card[3] / 2, card[2] + card[4] - 28, {size = 22, anchor = {0.5, 0.5}, outlineWidth = 3, outlineColor = '#FF15172A', maxWidth = card[3] - 20, wrap = false, minScale = 0.7, layer = 2})
end

return Playback
