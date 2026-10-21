-- Shake and flash: a hit on a knight adds trauma to the camera, whose shake grows with the square of the trauma and calms down as `traumaDecay` takes it away, covers the view with a fading color through `camera:flash` and turns the struck knight white for a moment through the `flash` color of its draw.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')
local Scenery = require('categories.effects.scenery')

local Hits = haylen.class('Hits', ParticleTest)

Hits.code = [[
camera.traumaDecay, camera.maxShakeOffset = 1.5, 48
camera:addTrauma(0.6)  -- The shake grows with the square of the trauma.
camera:flash('#B0FFFFFF', 0.3)
camera:update(dt)
graphics2d.draw(knight, x, y, {pivotY = 1, flash = {1, 1, 1, flash}})  -- Turns the knight white.]]

Hits.kinds = {
    {id = 'light', text = 'Light hit, a little trauma', trauma = 0.25},
    {id = 'heavy', text = 'Heavy hit, a white flash', trauma = 0.6, flash = '#B0FFFFFF'},
    {id = 'damage', text = 'Damage, a red flash', trauma = 0.45, flash = '#90FF2020'},
    {id = 'blast', text = 'Blast, full trauma', trauma = 1, flash = '#E0FFE8B0'},
}
Hits.knights = {-440, 0, 440}
Hits.knightScale = 1.3
Hits.spriteFlashTime = 0.16
Hits.knockDistance = 46

function Hits:init(entry)
    Hits.super.init(self, entry)
    self.scenery = Scenery()
    self.knight = assets.texture('sprites/images/hero.png', {filter = 'linear'})
    self.camera.maxShakeOffset = 48
    self.camera.maxShakeAngle = 0.05
    self.kind = Hits.kinds[2]
    self.flashLength = 0.3
    self.screenFlash, self.spriteFlash = true, true
    self.hits = 0
    self.knightStates = {}
    for index, x in ipairs(Hits.knights) do
        self.knightStates[index] = {x = x, flash = 0, knock = 0, side = 1}
    end
end

function Hits:enter()
    local camera = self.camera
    local kinds = {}
    for index, kind in ipairs(Hits.kinds) do
        kinds[index] = {id = kind.id, text = kind.text}
    end
    self:frame{
        hint = 'A click, a tap, E, Enter, Space or the south button hits the knight nearest the cursor. The meter in the corner shows the trauma of the camera.',
        code = Hits.code,
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
            ui.toggle{id = 'sprite', text = 'Flash the knight', checked = self.spriteFlash, onChange = function(event) self.spriteFlash = event.checked end},
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
    for _, knight in ipairs(self.knightStates) do
        local away = math.abs(knight.x - self.cursorX)
        if away < distance then
            best, distance = knight, away
        end
    end
    return best
end

function Hits:hit(knight)
    local kind = self.kind
    self.hits = self.hits + 1
    self.camera:addTrauma(kind.trauma)
    if kind.flash and self.screenFlash then
        self.camera:flash(kind.flash, self.flashLength)
    end
    if self.spriteFlash then
        knight.flash = Hits.spriteFlashTime
    end
    knight.knock = 1
    knight.side = self.cursorX <= knight.x and 1 or -1
end

function Hits:update(dt)
    Hits.super.update(self, dt)
    self.scenery:update(dt)
    if self.stage and self:pressed() then
        self:hit(self:nearest())
    end
    for _, knight in ipairs(self.knightStates) do
        knight.flash = math.max(0, knight.flash - dt)
        knight.knock = knight.knock * math.exp(-10 * dt)
    end

    local camera = self.camera
    camera:update(dt)
    local offset = camera:shakeOffset()
    self:report('Trauma %.2f, shake of %.1f units and %.1f degrees, view flash %.2f, hits %d', camera.trauma, math.sqrt(offset.x * offset.x + offset.y * offset.y), math.abs(math.deg(camera:renderRotation())), camera:flashColor().a, self.hits)
end

function Hits:draw(area)
    self.scenery:draw()
    for index, knight in ipairs(self.knightStates) do
        local x = knight.x + knight.side * knight.knock * Hits.knockDistance
        local flash = knight.flash / Hits.spriteFlashTime
        graphics2d.draw(self.knight, x, Scenery.ground, {scaleX = Hits.knightScale, scaleY = Hits.knightScale, pivotY = 1, rotation = knight.side * knight.knock * 0.12, flash = {1, 1, 1, flash}, layer = 2})
        ParticleTest.label(string.format('Knight %d', index), knight.x, Scenery.ground + 20)
    end
end

-- The trauma meter stays still in a corner of the play area while the view shakes: a plate with its name and a pill that fills with the trauma.
function Hits:renderUi()
    Hits.super.renderUi(self)
    local stage = self.stage
    if not stage then
        return
    end
    graphics2d.beginScreen()
    local width = m.clamp(stage.width - 48, 0, 360)
    local x, y = stage.x + 24, stage:bottom() - 88
    graphics2d.drawShape({x, y, width, 64}, {radius = 18, color = '#C8151824', borderWidth = 2, borderColor = '#FF3A4160'})
    graphics2d.drawText(nil, 'Trauma', x + 18, y + 10, {size = 18, color = '#FFE8EAF2', layer = 1})
    graphics2d.drawText(nil, string.format('%.0f%%', self.camera.trauma * 100), x + width - 18, y + 10, {size = 18, color = '#FFAAB4CC', anchor = {1, 0}, layer = 1})
    local bar = {x + 18, y + 38, width - 36, 14}
    graphics2d.drawShape(bar, {radius = 7, color = '#FF2A3046', layer = 1})
    if self.camera.trauma > 0 then
        graphics2d.drawShape({bar[1], bar[2], math.max(bar[4], bar[3] * self.camera.trauma), bar[4]}, {radius = 7, color = '#FFFF7A4A', layer = 2})
    end
end

return Hits
