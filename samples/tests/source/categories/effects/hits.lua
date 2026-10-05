-- Shake and flash: a hit on a robot adds trauma to the camera, whose shake grows with the square of the trauma and calms down as `traumaDecay` takes it away, covers the view with a fading color through `camera:flash` and turns the struck robot white for a moment through the `flash` color of its draw.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')
local Scenery = require('categories.effects.scenery')

local Hits = haylen.class('Hits', ParticleTest)

Hits.kinds = {
    {id = 'light', text = 'Light hit, a little trauma', trauma = 0.25},
    {id = 'heavy', text = 'Heavy hit, a white flash', trauma = 0.6, flash = '#B0FFFFFF'},
    {id = 'damage', text = 'Damage, a red flash', trauma = 0.45, flash = '#90FF2020'},
    {id = 'blast', text = 'Blast, full trauma', trauma = 1, flash = '#E0FFE8B0'},
}
Hits.robots = {-440, 0, 440}
Hits.robotScale = 2.6
Hits.spriteFlashTime = 0.16
Hits.knockDistance = 46

function Hits:init(entry)
    Hits.super.init(self, entry)
    self.scenery = Scenery()
    self.robot = assets.texture('sprites/images/hero.png', {filter = 'linear'})
    self.camera.maxShakeOffset = 48
    self.camera.maxShakeAngle = 0.05
    self.kind = Hits.kinds[2]
    self.flashLength = 0.3
    self.screenFlash, self.spriteFlash = true, true
    self.hits = 0
    self.robotStates = {}
    for index, x in ipairs(Hits.robots) do
        self.robotStates[index] = {x = x, flash = 0, knock = 0, side = 1}
    end
end

function Hits:enter()
    local camera = self.camera
    local kinds = {}
    for index, kind in ipairs(Hits.kinds) do
        kinds[index] = {id = kind.id, text = kind.text}
    end
    self:frame{
        hint = 'A click, a tap, E, Enter, Space or the south button hits the robot nearest the cursor.',
        cursor = true,
        controls = {
            ui.formField{label = 'Hit', ui.radioGroup{id = 'kind', items = kinds, selected = self.kind.id, onChange = function(event)
                for _, kind in ipairs(Hits.kinds) do
                    if kind.id == event.value then
                        self.kind = kind
                    end
                end
            end}},
            ui.toggle{id = 'screen', text = 'Flash the view', checked = self.screenFlash, onChange = function(event) self.screenFlash = event.checked end},
            ui.toggle{id = 'sprite', text = 'Flash the robot', checked = self.spriteFlash, onChange = function(event) self.spriteFlash = event.checked end},
            ui.formField{label = 'Trauma lost per second', ui.slider{id = 'decay', min = 0.2, max = 4, value = camera.traumaDecay, showValue = true, onChange = function(event)
                camera.traumaDecay = event.value
            end}},
            ui.formField{label = 'Largest offset', ui.slider{id = 'offset', min = 0, max = 120, value = camera.maxShakeOffset, showValue = true, decimals = 0, onChange = function(event)
                camera.maxShakeOffset = event.value
            end}},
            ui.formField{label = 'Flash length in seconds', ui.slider{id = 'length', min = 0.05, max = 1, value = self.flashLength, showValue = true, onChange = function(event)
                self.flashLength = event.value
            end}},
            ui.button{id = 'recoil', text = 'Recoil to the left', onClick = function() camera:shake(0.7, -1, 0) end},
        },
    }
end

function Hits:nearest()
    local best, distance = nil, math.huge
    for _, robot in ipairs(self.robotStates) do
        local away = math.abs(robot.x - self.cursorX)
        if away < distance then
            best, distance = robot, away
        end
    end
    return best
end

function Hits:hit(robot)
    local kind = self.kind
    self.hits = self.hits + 1
    self.camera:addTrauma(kind.trauma)
    if kind.flash and self.screenFlash then
        self.camera:flash(kind.flash, self.flashLength)
    end
    if self.spriteFlash then
        robot.flash = Hits.spriteFlashTime
    end
    robot.knock = 1
    robot.side = self.cursorX <= robot.x and 1 or -1
end

function Hits:update(dt)
    Hits.super.update(self, dt)
    self.scenery:update(dt)
    if self.stage and self:pressed() then
        self:hit(self:nearest())
    end
    for _, robot in ipairs(self.robotStates) do
        robot.flash = math.max(0, robot.flash - dt)
        robot.knock = robot.knock * math.exp(-10 * dt)
    end

    local camera = self.camera
    camera:update(dt)
    local offset = camera:shakeOffset()
    self:report('Trauma %.2f   Shake %.1f, %.1f   Angle %.3f   View flash %.2f   Hits %d', camera.trauma, offset.x, offset.y, camera:renderRotation(), camera:flashColor().a, self.hits)
end

function Hits:draw(area)
    self.scenery:draw()
    for index, robot in ipairs(self.robotStates) do
        local x = robot.x + robot.side * robot.knock * Hits.knockDistance
        local flash = robot.flash / Hits.spriteFlashTime
        graphics2d.draw(self.robot, x, Scenery.ground, {scaleX = Hits.robotScale, scaleY = Hits.robotScale, pivotY = 1, rotation = robot.side * robot.knock * 0.12, flash = {1, 1, 1, flash}, layer = 2})
        ParticleTest.label(string.format('Robot %d', index), robot.x, Scenery.ground + 20)
    end
end

-- The trauma bar stays still in a corner of the play area while the view shakes.
function Hits:renderUi()
    Hits.super.renderUi(self)
    local stage = self.stage
    if not stage then
        return
    end
    graphics2d.beginScreen()
    local x, y = stage.x + 24, stage:bottom() - 48
    local width = m.clamp(stage.width - 48, 0, 400)
    graphics2d.drawRect({x, y, width, 24}, '#80000000')
    graphics2d.drawRect({x, y, width * self.camera.trauma, 24}, '#FFFF6040', {layer = 1})
end

return Hits
