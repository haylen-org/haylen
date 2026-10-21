-- The function `video.load` opens a video file of the package with the decoder of the platform, and the texture of the video draws like any other: as two sprites on the stage, one of them turning, and as an image of the UI that `ui.setImage` names. The sound plays on the music bus.
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

function Playback:enter()
    self.events = {}
    self.clip = video.load('video/clip.mp4', {autoplay = true, loop = true, owner = self})
    self.clip:on('ready', function()
        self:note('Event "ready", ' .. self.clip.width .. ' x ' .. self.clip.height .. ' pixels, ' .. string.format('%.2f', self.clip.duration) .. ' seconds')
        ui.setImage('@clip', self.clip.texture)
        self:set('picture', {visible = true})
    end, {owner = self})
    self.clip:on('ended', function() self:note('Event "ended"') end, {owner = self})
    self.clip:on('error', function(error) self:note('Event "error" with the code "' .. error.code .. '": ' .. error.message) end, {owner = self})
    self:frame{
        code = Playback.code,
        hint = 'The square crosses the screen once a second and the tone rises with it.',
        controls = {
            ui.sectionTitle{text = 'Playback'},
            ui.row{wrap = true, gap = 8,
                ui.button{text = 'Play', onClick = function() self.clip:play() end},
                ui.button{text = 'Pause', onClick = function() self.clip:pause() end},
                ui.button{text = 'Start', onClick = function() self.clip:seek(0) end},
            },
            ui.toggle{text = 'Loop', checked = true, onChange = function(event) self.clip.loop = event.checked end},
            ui.label{text = 'Rate', color = 'textMuted'},
            ui.segmentedControl{selected = '1', items = {{id = '0.5', text = '0.5x'}, {id = '1', text = '1x'}, {id = '2', text = '2x'}}, onChange = function(event) self.clip.rate = tonumber(event.value) end},
            ui.label{text = 'Volume', color = 'textMuted'},
            ui.slider{value = 1, onChange = function(event) self.clip.volume = event.value end},
            ui.toggle{text = 'Muted', onChange = function(event) self.clip.muted = event.checked end},
            ui.sectionTitle{text = 'As an image of the UI'},
            ui.image{id = 'picture', image = '@clip', width = 320, height = 180, radius = 12, visible = false},
            ui.sectionTitle{text = 'Events'},
            ui.label{id = 'events', text = 'Loading the clip.', color = 'textMuted'},
        },
        panelWidth = 520,
    }
end

function Playback:note(line)
    table.insert(self.events, 1, line)
    self.events[5] = nil
    self:set('events', {text = table.concat(self.events, '\n')})
end

function Playback:exit()
    ui.removeImage('@clip')
    Playback.super.exit(self)
end

function Playback:update(dt)
    Playback.super.update(self, dt)
    local clip = self.clip
    self:status(string.format('State "%s", time %.2f of %.2f seconds, frame %d, rate %gx, %s.', clip.state, clip.time, clip.duration, clip.frameCount, clip.rate, clip.playing and 'playing' or 'paused'))
end

function Playback:draw(area)
    local width = math.min(area.width * 0.6, 640)
    local height = width * 9 / 16
    graphics2d.draw(self.clip.texture, area.width / 2, area.height * 0.4, {width = width, height = height})
    graphics2d.draw(self.clip.texture, area.width * 0.8, area.height * 0.82, {width = width / 3, height = height / 3, rotation = haylen.elapsed() * 0.5})
    Test.caption('A sprite of the texture of the video, and a smaller one that turns', area.width / 2, area.height * 0.4 + height / 2 + 16, {anchor = {0.5, 0}})
end

return Playback
