-- Playback controls: one tween that keeps itself after it completes, driven by buttons and a seek slider, with its state and its callbacks counted on the panel in the same frame, and the button of the state it is in highlighted.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local Controls = haylen.class('Controls', TweenTest)

-- The buttons that show a state of the tween, which the panel highlights while the tween is in it.
local kStates = {'play', 'pause', 'complete', 'kill'}
local kCode = [[
local handle = tween.to(box, 3, {x = 1000, angle = math.pi * 2, color = '#FF3DBE7A'}, {autoKill = false})
handle:pause()  -- And play, resume and restart.
handle:reverse()
handle.progress = 0.5  -- Seeks.
handle:complete()
handle:kill()]]

function Controls:enter()
    self.star = TweenTest.icon('star')
    self:frame({
        hint = 'Drag the slider to seek, which pauses the tween. A killed tween leaves the engine, so New builds another one.',
        code = kCode,
        controls = {
            ui.row{wrap = true, gap = 12,
                ui.button{id = 'play', text = 'Play', grow = 1, onClick = function() self.handle:play() end},
                ui.button{id = 'pause', text = 'Pause', grow = 1, onClick = function() self.handle:pause() end},
            },
            ui.row{wrap = true, gap = 12,
                ui.button{id = 'resume', text = 'Resume', grow = 1, onClick = function() self.handle:resume() end},
                ui.button{id = 'restart', text = 'Restart', grow = 1, onClick = function() self.handle:restart() end},
            },
            ui.row{wrap = true, gap = 12,
                ui.button{id = 'reverse', text = 'Reverse', grow = 1, onClick = function() self.handle:reverse() end},
                ui.button{id = 'complete', text = 'Complete', grow = 1, onClick = function() self.handle:complete() end},
            },
            ui.row{wrap = true, gap = 12,
                ui.button{id = 'kill', text = 'Kill', grow = 1, onClick = function() self.handle:kill() end},
                ui.button{id = 'new', text = 'New', grow = 1, onClick = function() self:build() end},
            },
            ui.formField{label = 'Seek', ui.slider{id = 'seek', min = 0, max = 1, value = 0, showValue = true, onChange = function(event)
                self.handle:pause()
                self.handle.progress = event.value
            end}},
            ui.sectionTitle{text = 'State'},
            ui.label{id = 'state', text = ''},
            ui.sectionTitle{text = 'Callbacks'},
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

-- The state the tween is in, as the id of the button that puts it there.
function Controls:current()
    local handle = self.handle
    if not handle.alive then
        return 'kill'
    end
    if handle.completed then
        return 'complete'
    end
    return handle.paused and 'pause' or 'play'
end

function Controls:update(dt)
    Controls.super.update(self, dt)
    -- The slider follows the progress whenever it changes, so it shows the end once the tween completes.
    local handle = self.handle
    if handle.progress ~= self.shownProgress then
        self.shownProgress = handle.progress
        self:set('seek', {value = handle.progress})
    end
    local current = self:current()
    if current ~= self.shownState then
        self.shownState = current
        for _, id in ipairs(kStates) do
            self:set(id, {variant = id == current and (id == 'kill' and 'destructive' or 'primary') or 'default'})
        end
    end
    local state = string.format('%s, %s, %s\nProgress %.2f, time %.2f s', handle.alive and 'Alive' or 'Killed', handle.playing and 'playing' or handle.paused and 'paused' or 'stopped', handle.reversed and 'backwards' or 'forward', handle.progress, handle.time)
    local calls = self.calls
    self:set('state', {text = state})
    self:set('calls', {text = string.format('Calls of "onStart" %d and "onUpdate" %d\nCalls of "onComplete" %d and "onKill" %d', calls.start, calls.update, calls.complete, calls.kill)})
end

-- A thick track that fills with the progress, its marks right under it, and the box that the tween moves, turns and colors.
function Controls:draw(area)
    local left, right, y = 160, area.width - 160, area.height / 2
    local progress = self.handle.progress
    graphics2d.drawShape({left - 12, y - 12, right - left + 24, 24}, {radius = 12, color = TweenTest.groove, borderWidth = 2, borderColor = TweenTest.grooveEdge})
    graphics2d.drawShape({left - 12, y - 12, (right - left) * progress + 24, 24}, {radius = 12, color = m.color(Test.accent):withAlpha(0.7), layer = 1})
    for _, mark in ipairs({0, 0.5, 1}) do
        local x = left + (right - left) * mark
        graphics2d.drawShape({x - 2, y + 24, 4, 14}, {radius = 2, color = TweenTest.tick})
        graphics2d.drawText(nil, string.format('Progress %g', mark), x, y + 44, {size = 22, color = Test.muted, anchor = {0.5, 0}})
    end
    local x = left + (right - left) * self.box.x / 1000
    local alpha = self.handle.alive and 1 or 0.35
    graphics2d.drawShape({x - 70, y - 64, 140, 140}, {radius = 30, rotation = self.box.angle, color = m.color(0, 0, 0, 0.3 * alpha), softness = 18, layer = 1})
    graphics2d.drawShape({x - 70, y - 70, 140, 140}, {radius = 30, rotation = self.box.angle, color = self.box.color:withAlpha(alpha), borderWidth = 5, borderColor = m.color('#FF1E1B33'):withAlpha(alpha), layer = 2})
    graphics2d.draw(self.star, x, y, {width = 84, height = 84, rotation = self.box.angle, color = m.color(1, 1, 1, alpha), layer = 3})
    if not self.handle.alive then
        graphics2d.drawText(nil, 'Killed', x, y - 120, {size = 34, color = Test.red, anchor = {0.5, 0.5}})
    end
end

return Controls
