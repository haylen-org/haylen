-- Playback controls: one tween that keeps itself after it completes, driven by buttons and a seek slider, with its state and its callbacks counted.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local Controls = haylen.class('Controls', sample.Test)

local kCode = [[
local handle = tween.to(box, 3, {x = 1000, angle = math.pi * 2, color = '#FF3DBE7A'}, {autoKill = false, onComplete = ..., onKill = ...})
handle:play()  handle:pause()  handle:resume()  handle:restart()  handle:reverse()
handle:complete()  handle:kill()  handle.progress = 0.5  -- seeks]]

function Controls:enter()
    self:frame({
        hint = 'Drag the slider to seek, which pauses the tween. A killed tween leaves the engine, so New builds another one.',
        code = kCode,
        controls = {
            ui.row{gap = 12,
                ui.button{id = 'play', text = 'Play', grow = 1, variant = 'primary', onClick = function() self.handle:play() end},
                ui.button{id = 'pause', text = 'Pause', grow = 1, onClick = function() self.handle:pause() end},
            },
            ui.row{gap = 12,
                ui.button{id = 'resume', text = 'Resume', grow = 1, onClick = function() self.handle:resume() end},
                ui.button{id = 'restart', text = 'Restart', grow = 1, onClick = function() self.handle:restart() end},
            },
            ui.row{gap = 12,
                ui.button{id = 'reverse', text = 'Reverse', grow = 1, onClick = function() self.handle:reverse() end},
                ui.button{id = 'complete', text = 'Complete', grow = 1, onClick = function() self.handle:complete() end},
            },
            ui.row{gap = 12,
                ui.button{id = 'kill', text = 'Kill', grow = 1, variant = 'destructive', onClick = function() self.handle:kill() end},
                ui.button{id = 'new', text = 'New', grow = 1, onClick = function() self:build() end},
            },
            ui.formField{label = 'Seek', ui.slider{id = 'seek', min = 0, max = 1, value = 0, showValue = true, onChange = function(event)
                self.handle:pause()
                self.handle.progress = event.value
            end}},
            ui.label{id = 'calls', text = '', font = 'caption'},
        },
        focus = 'play',
    })
    self:build()
end

function Controls:build()
    if self.handle then
        self.handle:kill()
    end
    self.box = {x = 0, angle = 0, color = m.color('#FF4C7DFF')}
    self.calls = {start = 0, update = 0, complete = 0, kill = 0}
    local calls = self.calls
    self.handle = tween.to(self.box, 3, {x = 1000, angle = math.pi * 2, color = '#FF3DBE7A'}, {
        owner = self,
        ease = 'sineInOut',
        autoKill = false,
        onStart = function() calls.start = calls.start + 1 end,
        onUpdate = function() calls.update = calls.update + 1 end,
        onComplete = function() calls.complete = calls.complete + 1 end,
        onKill = function() calls.kill = calls.kill + 1 end,
    })
end

function Controls:update(dt)
    Controls.super.update(self, dt)
    local handle = self.handle
    if handle.playing then
        self:set('seek', {value = handle.progress})
    end
    local calls = self.calls
    self:set('calls', {text = string.format('onStart %d   onUpdate %d\nonComplete %d   onKill %d', calls.start, calls.update, calls.complete, calls.kill)})
    self:status(string.format('alive %s  playing %s  paused %s  reversed %s  completed %s  progress %.2f  time %.2f', handle.alive, handle.playing, handle.paused, handle.reversed, handle.completed, handle.progress, handle.time))
end

function Controls:draw(area)
    local left, right, y = 120, area.width - 120, area.height / 2
    graphics2d.drawLine(left, y, right, y, 4, sample.line)
    local x = left + (right - left) * self.box.x / 1000
    local points = {}
    for corner = 0, 3 do
        local turn = self.box.angle + math.pi / 4 + corner * math.pi / 2
        points[corner + 1] = {x + math.cos(turn) * 70, y + math.sin(turn) * 70}
    end
    graphics2d.drawPolygon(points, self.box.color, {layer = 1})
    graphics2d.drawText(nil, self.handle.alive and '' or 'killed', area.width / 2, y + 140, {size = 40, color = sample.red, anchor = {0.5, 0.5}})
end

return Controls
