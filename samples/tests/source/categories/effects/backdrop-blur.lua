-- Backdrop blur: `graphics2d.drawGlass` with only a `blur` blurs whatever the canvases drew behind a region this frame. The region follows the cursor over the moving landscape, and its radius, corners and size change on the controls, while the status shows that a wider blur adds only a small pass.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')
local Scenery = require('categories.effects.scenery')

local BackdropBlur = haylen.class('BackdropBlur', ParticleTest)

BackdropBlur.code = [[
graphics2d.beginWorld(camera)
scenery:draw()  -- What the region blurs.
-- Only a blur: the same few passes over the region whatever its radius.
graphics2d.drawGlass({x - 360, y - 200, 720, 400}, {blur = 32, radius = 48})]]

function BackdropBlur:init(entry)
    BackdropBlur.super.init(self, entry)
    self.scenery = Scenery()
    self.knight = assets.texture('sprites/images/hero.png', {filter = 'linear'})
    self.blur, self.radius, self.width, self.height = 32, 48, 720, 400
    self.x, self.y = 0, 0
    self.pinned = false
end

function BackdropBlur:enter()
    self:frame{
        hint = 'The blurred region follows the cursor. A click, a tap, E, Enter, Space or the south button pins it in place or lets it follow again.',
        code = BackdropBlur.code,
        cursor = true,
        controls = {
            ui.formField{label = 'Blur radius', ui.slider{id = 'blur', min = 0, max = 160, value = self.blur, showValue = true, decimals = 0, onChange = function(event) self.blur = event.value end}},
            ui.formField{label = 'Corner radius', ui.slider{id = 'radius', min = 0, max = 200, value = self.radius, showValue = true, decimals = 0, onChange = function(event) self.radius = event.value end}},
            ui.formField{label = 'Width', ui.slider{id = 'width', min = 160, max = 1600, value = self.width, showValue = true, decimals = 0, onChange = function(event) self.width = event.value end}},
            ui.formField{label = 'Height', ui.slider{id = 'height', min = 120, max = 900, value = self.height, showValue = true, decimals = 0, onChange = function(event) self.height = event.value end}},
        },
    }
end

function BackdropBlur:update(dt)
    BackdropBlur.super.update(self, dt)
    self.scenery:update(dt)
    if self.stage and self:pressed() then
        self.pinned = not self.pinned
    end
    if not self.pinned then
        self.x, self.y = self.cursorX, self.cursorY
    end
    local stats = graphics2d.stats()
    self:report('The region blurs over %.0f units, and the frame takes %d passes and %d draw calls.', self.blur, stats.passes, stats.drawCalls)
end

function BackdropBlur:draw(area)
    self.scenery:draw()
    local bob = math.sin(self.scenery.time * 2) * 30
    graphics2d.draw(self.knight, -200, Scenery.ground + bob * 0.2, {scaleX = 1.3, scaleY = 1.3, pivotY = 1, layer = 1})
    for index = 0, 4 do
        graphics2d.drawCircle(-600 + index * 300, -120 + math.sin(self.scenery.time * 1.5 + index) * 120, 70, index % 2 == 0 and '#FFFFC93C' or '#FF2F86F6', {layer = 2})
    end
    graphics2d.drawGlass({self.x - self.width / 2, self.y - self.height / 2, self.width, self.height}, {blur = self.blur, radius = self.radius, layer = 10})
end

return BackdropBlur
