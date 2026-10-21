-- Skeletons: a cutout puppet whose walk and idle crossfade on one track, whose back arm waves on a second track over them, whose cape mesh flaps on a third, whose front arm reaches for the pointer through inverse kinematics and whose cap a skin swaps for a crown, with the footsteps its walk reports. While it walks the ground scrolls under it. A row of smaller puppets walks the same skeleton at other speeds and skins along a far ledge.
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AnimationTest = require('categories.animation.animation-test')
local Backdrop = require('categories.animation.backdrop')
local Pointer = require('harness.pointer')

local Skeletons = haylen.class('Skeletons', AnimationTest)

Skeletons.world = {960, 540}
Skeletons.ground = 205
Skeletons.ledge = {ground = 80, height = 170}
Skeletons.scale = 1.45
Skeletons.rowScale = 0.72
Skeletons.lines = 5
-- The walk carries a puppet of scale 1 this many units a second, so its feet keep to the ground.
Skeletons.stride = 165
Skeletons.fade = 0.25
Skeletons.actions = {actions = Pointer.actions}
Skeletons.doing = {idle = 'stands idle', walk = 'walks', wave = 'waves', flap = 'flaps the cape'}

Skeletons.code = [[
local puppet = animation2d.newSkeleton(assets.load('animation/puppet.skeleton'), {scaleX = 1.45, scaleY = 1.45})
puppet:crossFade('walk', 0.25, {track = 1})
puppet:play('wave', {track = 2})
puppet:setIkTarget('reach', x, y)
puppet.skin = 'royal'
puppet:on('event', function(event) self:record(event) end, {owner = self})]]

function Skeletons:enter()
    self.data = assets.load('animation/puppet.skeleton', 'skeleton', AnimationTest.linear)
    self.backdrop = Backdrop()
    self.puppet = animation2d.newSkeleton(self.data, {x = -170, y = Skeletons.ground, scaleX = Skeletons.scale, scaleY = Skeletons.scale})
    self.puppet:play('idle', {track = 1})
    self.puppet:play('flap', {track = 3})
    self.walking = false
    self.aiming = false
    self.pace = 0
    self.offset = 0
    self.history = {}
    self.pointer = Pointer()
    self.puppet:on('event', function(event) self:record(event) end, {owner = self})
    self.row = {}
    for index = 1, 4 do
        local small = animation2d.newSkeleton(self.data, {skin = index % 2 == 0 and 'royal' or 'default', y = Skeletons.ledge.ground, scaleX = Skeletons.rowScale, scaleY = Skeletons.rowScale})
        small:play('walk', {track = 1})
        small:play('flap', {track = 3})
        self.row[index] = {skeleton = small, speed = 0.7 + index * 0.15, place = index * 260}
    end
    self:loadActions(Skeletons.actions)
    self:frame{
        code = Skeletons.code,
        play = true,
        hint = 'Turn on the reach and move the pointer or the right stick over the play area, and switch the tracks with the controls.',
        controls = {
            ui.toggle{id = 'walk', text = 'Walk, crossfading from idle', onChange = function(event)
                self.walking = event.checked
                self.puppet:crossFade(event.checked and 'walk' or 'idle', Skeletons.fade, {track = 1})
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
                self.pointer.shown = event.checked
                self.puppet:setIkMix('reach', event.checked and 1 or 0)
            end},
            ui.toggle{id = 'royal', text = 'Wear the crown', onChange = function(event) self.puppet.skin = event.checked and 'royal' or 'default' end},
            ui.sectionTitle{text = 'Footsteps'},
            ui.column{id = 'history', gap = 4, ui.label{text = 'Turn on the walk to list the steps of its feet.', color = 'textMuted'}},
        },
        focus = 'walk',
    }
end

function Skeletons:record(event)
    table.insert(self.history, 1, string.format('%s%s foot steps, %s on track %d', event.text:sub(1, 1):upper(), event.text:sub(2), event.animation, event.track))
    self.history[Skeletons.lines + 1] = nil
    local rows = {}
    for index, text in ipairs(self.history) do
        rows[index] = ui.label{text = text, color = index == 1 and 'text' or 'textMuted'}
    end
    self.gui:replaceChildren('history', rows)
end

function Skeletons:update(dt)
    Skeletons.super.update(self, dt)
    if not self.area then
        return
    end
    Backdrop.frame(self, Skeletons.world, Skeletons.ground, 0.85)
    self.pointer:update(dt, self)
    if self.aiming then
        self.puppet:setIkTarget('reach', self.pointer.worldX, self.pointer.worldY)
    end
    self.puppet:update(dt)

    -- The ground scrolls under the walking puppet at the pace of its feet, easing in and out with the crossfade.
    self.pace = m.moveToward(self.pace, self.walking and 1 or 0, dt / Skeletons.fade)
    self.offset = self.offset + self.pace * Skeletons.stride * Skeletons.scale * dt

    -- The row walks along the far ledge, which scrolls slower, and comes back on the other side of the view.
    local bounds = self.camera:visibleBounds()
    local left, span = bounds.x - 120, bounds.width + 240
    for _, entry in ipairs(self.row) do
        entry.place = entry.place + Skeletons.stride * Skeletons.rowScale * entry.speed * dt
        entry.skeleton.x = left + (entry.place - self.offset * 0.6 - left) % span
        entry.skeleton:update(dt * entry.speed)
    end
    self:status(string.format('Track 1 %s, track 2 %s and track 3 %s.', Skeletons.doing[self.puppet:trackAnimation(1)] or 'is empty', Skeletons.doing[self.puppet:trackAnimation(2)] or 'is empty', Skeletons.doing[self.puppet:trackAnimation(3)] or 'is empty'))
end

function Skeletons:draw(area)
    self.backdrop:draw(graphics2d.canvasBounds(), Skeletons.ground, 300, self.offset, Skeletons.ledge)
    for _, entry in ipairs(self.row) do
        Backdrop.shadow(entry.skeleton.x, Skeletons.ledge.ground, 70, -21)
        entry.skeleton:draw({layer = -20})
    end
    Backdrop.shadow(self.puppet.x, Skeletons.ground, 150, 0)
    self.puppet:draw({layer = 1})
    local x, y = self.puppet:point('hand')
    if x then
        graphics2d.drawShape({x - 8, y - 8, 16, 16}, {kind = 'ellipse', color = '#FFFFC93C', borderWidth = 3, borderColor = '#FF1E1B33', layer = 5})
    end
end

function Skeletons:renderUi()
    self.pointer:draw()
end

return Skeletons
