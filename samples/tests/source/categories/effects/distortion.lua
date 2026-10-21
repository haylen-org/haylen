-- Heat haze and shock rings: draws with a `distortion` bend the image of a canvas with post-processing instead of drawing colors. Small rising blobs of the library effect `heat-haze` shimmer above a campfire, a press sends a ring of `shockwave-distortion` across the landscape from the cursor, the shield of a knight ripples with `shield-ripple-distortion`, and a glass bubble with a sharp rim follows the cursor.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')
local Scenery = require('categories.effects.scenery')

local Distortion = haylen.class('Distortion', ParticleTest)

Distortion.code = [[
graphics2d.beginWorld(camera, {postProcess = {distortion = 40}})  -- How far the distortion draws move pixels.
haze:draw()  -- The particles of `fire/heat-haze` bend the image instead of drawing colors.
ring:restart()  -- A ring of `explosions/shockwave-distortion` pushes the image outward.
graphics2d.draw(bubble, x, y, {width = 280, height = 280, distortion = 0.8})]]

Distortion.library = 'particles/library/'
Distortion.rings = 4
Distortion.fire = {-300, Scenery.ground}
Distortion.shield = {460, 250}
Distortion.shieldInterval = 1.4

function Distortion.newSystem(name)
    return particles2d.newSystem(assets.load(Distortion.library .. name .. '.particles'), {seed = 5})
end

function Distortion:init(entry)
    Distortion.super.init(self, entry)
    self.scenery = Scenery()
    self.knight = assets.texture('sprites/images/hero.png', {filter = 'linear'})
    self.firewood = assets.texture('effects/campfire.png', {filter = 'linear'})
    self.bubble = ParticleTest.look('hard-dot', {})
    self.campfire = Distortion.newSystem('fire/campfire-realistic')
    self.haze = Distortion.newSystem('fire/heat-haze')
    self.campfire.position, self.haze.position = {Distortion.fire[1], Distortion.fire[2] - 40}, {Distortion.fire[1], Distortion.fire[2] - 180}
    self.campfire.scale = 2
    self.shield = Distortion.newSystem('scifi/shield-ripple-distortion')
    self.shield.position = Distortion.shield
    self.rings = {}
    for index = 1, Distortion.rings do
        self.rings[index] = Distortion.newSystem('explosions/shockwave-distortion')
        self.rings[index].emitting = false
    end
    self.nextRing = 1
    self.shieldTime = 0
    self.post = {distortion = 40}
    self.hazeOn, self.shieldOn, self.bubbleOn = true, true, true
    self.bubbleSize, self.bubbleStrength = 280, 0.8
end

function Distortion:enter()
    self:frame{
        hint = 'A click, a tap, E, Enter, Space or the south button sends a shock ring from the cursor.',
        code = Distortion.code,
        cursor = true,
        controls = {
            ui.formField{label = 'Bend distance', ui.slider{id = 'distance', min = 0, max = 64, value = self.post.distortion, showValue = true, decimals = 0, onChange = function(event)
                self.post.distortion = event.value
            end}},
            ui.toggle{id = 'haze', text = 'Heat haze over the fire', checked = self.hazeOn, onChange = function(event) self.hazeOn = event.checked end},
            ui.toggle{id = 'shield', text = 'Shield ripples', checked = self.shieldOn, onChange = function(event) self.shieldOn = event.checked end},
            ui.toggle{id = 'bubble', text = 'Glass bubble at the cursor', checked = self.bubbleOn, onChange = function(event) self.bubbleOn = event.checked end},
            ui.formField{label = 'Bubble size', ui.slider{id = 'bubbleSize', min = 80, max = 520, value = self.bubbleSize, showValue = true, decimals = 0, onChange = function(event)
                self.bubbleSize = event.value
            end}},
            ui.formField{label = 'Bubble strength', ui.slider{id = 'bubbleStrength', min = 0.1, max = 1, value = self.bubbleStrength, showValue = true, onChange = function(event)
                self.bubbleStrength = event.value
            end}},
            ui.button{id = 'ring', text = 'Shock ring in the middle', onClick = function() self:sendRing(0, 0) end},
        },
    }
end

-- Rings come from a small pool, so a new ring reuses the oldest one.
function Distortion:sendRing(x, y)
    local ring = self.rings[self.nextRing]
    self.nextRing = self.nextRing % Distortion.rings + 1
    ring.position = {x, y}
    ring:restart()
end

function Distortion:update(dt)
    Distortion.super.update(self, dt)
    self.scenery:update(dt)
    if self.stage and self:pressed() then
        self:sendRing(self.cursorX, self.cursorY)
    end

    self.shieldTime = self.shieldTime + dt
    if self.shieldOn and self.shieldTime >= Distortion.shieldInterval then
        self.shieldTime = 0
        self.shield:restart()
    end
    self.haze.emitting = self.hazeOn
    self.campfire:update(dt)
    self.haze:update(dt)
    self.shield:update(dt)
    local rings = ParticleTest.updateAll(self.rings, dt)
    self:report('Bend distance %.0f, %d particles of haze, %d rings on the shield and %d shock rings', self.post.distortion, self.haze.count, self.shield.count, rings)
end

function Distortion:render()
    if not self.area then
        return
    end
    graphics2d.beginWorld(self.camera, {postProcess = self.post})
    self:draw(self.area)
end

function Distortion:draw(area)
    self.scenery:draw()
    graphics2d.draw(self.knight, Distortion.shield[1], Scenery.ground, {scaleX = 1.2, scaleY = 1.2, pivotY = 1, layer = 1})
    graphics2d.draw(self.firewood, Distortion.fire[1], Distortion.fire[2] + 6, {width = 256, height = 160, pivotY = 1, layer = -1})
    ParticleTest.label('Shield', Distortion.shield[1], Scenery.ground + 20)
    ParticleTest.label('Campfire', Distortion.fire[1], Scenery.ground + 20)
    self.campfire:draw()
    self.haze:draw()
    self.shield:draw()
    ParticleTest.drawAll(self.rings)
    if self.bubbleOn then
        graphics2d.draw(self.bubble.texture, self.cursorX, self.cursorY, {source = self.bubble.frames[1], width = self.bubbleSize, height = self.bubbleSize, distortion = self.bubbleStrength})
    end
end

return Distortion
