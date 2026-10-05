-- Render targets: an offscreen canvas with a hero orbiting a ring, drawn every frame and then used as a texture as it is, turned and scaled, tinted and flipped, and on a waving mesh.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local SpriteTest = require('categories.sprites.sprite-test')
local Test = require('harness.test')

local RenderTargets = haylen.class('RenderTargets', SpriteTest)

RenderTargets.size = 320
RenderTargets.columns = 16
RenderTargets.labels = {'As it is', 'Turned and scaled', 'Tinted and flipped', 'On a waving mesh'}

RenderTargets.code = [[
local target = graphics.newRenderTarget(320, 320, {filter = 'linear'})
graphics2d.beginTarget(target, targetCamera, {clear = '#FF203040'})  -- Draw anything, every frame or once.
graphics2d.beginWorld(camera)  graphics2d.draw(target.texture, x, y, {rotation = 0.3, color = '#FFFF9060', flipVertical = true})
graphics2d.drawMesh(target.texture, vertices, indices)  -- The same texture on a waving strip.]]

function RenderTargets:enter()
    local size, columns = RenderTargets.size, RenderTargets.columns
    self.target = graphics.newRenderTarget(size, size, {filter = 'linear'})
    self.targetCamera = graphics2d.newCamera()
    self.targetCamera.position = {size / 2, size / 2}
    self.hero = graphics2d.newSprite(SpriteTest.texture('images/hero.png'))
    self.time = 0
    self.redraw = true
    self.vertices, self.indices = {}, {}
    for column = 0, columns do
        self.vertices[column * 2 + 1] = {u = column / columns, v = 0}
        self.vertices[column * 2 + 2] = {u = column / columns, v = 1}
        if column < columns then
            local a = column * 2 + 1
            table.move({a, a + 1, a + 2, a + 1, a + 3, a + 2}, 1, 6, #self.indices + 1, self.indices)
        end
    end
    self:frame{
        code = RenderTargets.code,
        hint = 'Turn the redraw off and the target keeps the last frame it drew.',
        controls = {ui.toggle{id = 'redraw', text = 'Redraw every frame', checked = true, onChange = function(event) self.redraw = event.checked end}},
        focus = 'redraw',
    }
end

function RenderTargets:update(dt)
    RenderTargets.super.update(self, dt)
    self.time = self.time + dt
    self:status(string.format('Target %d x %d   %s', self.target.width, self.target.height, self.redraw and 'Drawn this frame' or 'Kept from before'))
end

function RenderTargets:drawTarget()
    local time, center = self.time, RenderTargets.size / 2
    graphics2d.beginTarget(self.target, self.targetCamera, {clear = '#FF203040'})
    for index = 0, 23 do
        local angle = index * 0.9 + time * 0.2
        graphics2d.drawCircle(center + math.cos(angle) * (60 + index * 4), center + math.sin(angle) * (60 + index * 4), 3, '#FFFFFFFF')
    end
    graphics2d.drawRing(center, center, 110, 6, '#FFF2B23A')
    self.hero.x, self.hero.y = center + math.cos(time * 1.5) * 90, center + math.sin(time * 1.5) * 90
    self.hero.rotation = time
    self.hero:draw()
    graphics2d.drawText(nil, string.format('%.1f', time), 16, 12, {size = 36, color = '#FF7FCBF2'})
end

-- The target draws before the stage, which then shows its texture.
function RenderTargets:render()
    if self.redraw then
        self:drawTarget()
    end
    RenderTargets.super.render(self)
end

function RenderTargets:draw(area)
    local texture, size = self.target.texture, math.min(area.width / 4.6, area.height * 0.55)
    local y = area.height * 0.42
    for index, label in ipairs(RenderTargets.labels) do
        Test.caption(label, area.width * (index - 0.5) / 4, y + size / 2 + 50, {size = 24, anchor = {0.5, 0.5}})
    end
    graphics2d.draw(texture, area.width * 0.125, y, {width = size, height = size})
    graphics2d.draw(texture, area.width * 0.375, y, {width = size, height = size, rotation = math.sin(self.time) * 0.5, scaleX = 0.8 + math.sin(self.time * 1.3) * 0.2, scaleY = 0.8 + math.sin(self.time * 1.3) * 0.2})
    graphics2d.draw(texture, area.width * 0.625, y, {width = size, height = size, color = '#FFFF9060', flipVertical = true})

    local columns = RenderTargets.columns
    local left, top = area.width * 0.875 - size / 2, y - size / 2
    for column = 0, columns do
        local wave = math.sin(self.time * 3 + column * 0.6) * 18
        self.vertices[column * 2 + 1].x, self.vertices[column * 2 + 1].y = left + size * column / columns, top + wave
        self.vertices[column * 2 + 2].x, self.vertices[column * 2 + 2].y = left + size * column / columns, top + size + wave
    end
    graphics2d.drawMesh(texture, self.vertices, self.indices)
end

return RenderTargets
