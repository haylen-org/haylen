-- Skeletons: a cutout puppet whose walk and idle crossfade on one track, whose back arm waves on a second track over them, whose cape mesh flaps on a third, whose front arm reaches for the pointer through inverse kinematics and whose hat a skin swaps, with the footsteps its walk reports. A row of smaller puppets walks the same skeleton at other speeds and skins.
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local AnimationTest = require('categories.animation.animation-test')
local Test = require('harness.test')

local Skeletons = haylen.class('Skeletons', AnimationTest)

Skeletons.lines = 5

Skeletons.code = [[
local puppet = animation2d.newSkeleton(assets.load('animation/puppet.skeleton'))
puppet:crossFade('walk', 0.25, {track = 1})  puppet:play('wave', {track = 2})
puppet:setIkTarget('reach', x, y)  puppet.skin = 'royal'
puppet:on('event', function(event) print(event.name, event.track, event.animation, event.text) end)]]

function Skeletons:enter()
    self.data = assets.load('animation/puppet.skeleton', 'skeleton', {filter = 'linear'})
    self.puppet = animation2d.newSkeleton(self.data, {scaleX = 1.5, scaleY = 1.5})
    self.puppet:play('idle', {track = 1})
    self.puppet:play('flap', {track = 3})
    self.walking = false
    self.aiming = false
    self.history = {}
    self.puppet:on('event', function(event) self:record(string.format('Event "%s" of "%s" on track %d, the %s foot', event.name, event.animation, event.track, event.text)) end, {owner = self})
    self.row = {}
    for index = 1, 4 do
        local small = animation2d.newSkeleton(self.data, {skin = index % 2 == 0 and 'royal' or 'default', scaleX = 0.9, scaleY = 0.9})
        small:play('walk', {track = 1})
        small:setTrackWeight(1, 1)
        self.row[index] = {skeleton = small, speed = 0.6 + index * 0.2}
    end
    self:frame{
        code = Skeletons.code,
        hint = 'Move the pointer over the stage while aiming, and switch the tracks with the controls.',
        controls = {
            ui.toggle{id = 'walk', text = 'Walk, crossfading from idle', onChange = function(event)
                self.walking = event.checked
                self.puppet:crossFade(event.checked and 'walk' or 'idle', 0.25, {track = 1})
            end},
            ui.toggle{id = 'wave', text = 'Wave on a second track', onChange = function(event)
                if event.checked then
                    self.puppet:crossFade('wave', 0.2, {track = 2})
                else
                    self.puppet:stop(2)
                end
            end},
            ui.toggle{id = 'aim', text = 'Reach for the pointer', onChange = function(event)
                self.aiming = event.checked
                self.puppet:setIkMix('reach', event.checked and 1 or 0)
            end},
            ui.toggle{id = 'royal', text = 'Royal skin', onChange = function(event) self.puppet.skin = event.checked and 'royal' or 'default' end},
            ui.column{id = 'history', gap = 4},
        },
        focus = 'walk',
    }
end

function Skeletons:record(line)
    table.insert(self.history, 1, line)
    self.history[Skeletons.lines + 1] = nil
    local rows = {}
    for index, text in ipairs(self.history) do
        rows[index] = ui.label{text = text, font = 'caption', color = index == 1 and 'text' or 'textMuted'}
    end
    self.gui:replaceChildren('history', rows)
end

function Skeletons:update(dt)
    Skeletons.super.update(self, dt)
    if not self.area then
        return
    end
    self.puppet.x = self.area.width * 0.35
    self.puppet.y = self.area.height * 0.75
    if self.aiming then
        self.puppet:setIkTarget('reach', self:toStage(input.mousePosition()))
    end
    self.puppet:update(dt)
    for index, entry in ipairs(self.row) do
        entry.skeleton.x = self.area.width * 0.62 + index * 90
        entry.skeleton.y = self.area.height * 0.9
        entry.skeleton:update(dt * entry.speed)
    end
    local bounds = self.puppet:bounds()
    self:status(string.format('Track 1 %s   Track 2 %s   Puppet %.0f x %.0f   Draw calls %d', self.puppet:trackAnimation(1) or 'none', self.puppet:trackAnimation(2) or 'none', bounds.width, bounds.height, graphics2d.stats().drawCalls))
end

function Skeletons:draw(area)
    graphics2d.drawRect({0, self.puppet.y, area.width, 4}, Test.line)
    self.puppet:draw()
    for _, entry in ipairs(self.row) do
        entry.skeleton:draw()
    end
    local x, y = self.puppet:point('hand')
    if x then
        graphics2d.drawCircle(x, y, 5, Test.warm, {layer = 5})
    end
end

return Skeletons
