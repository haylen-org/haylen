-- Pooled bullets: a crystal turret in a clearing at night fires a bullet hell whose projectiles come from an object pool, so firing thousands of them creates no garbage, and draw in one batch of glows with an additive blend.
local collections = require('haylen.collections')
local debugging = require('haylen.debug')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local localization = require('haylen.localization')
local m = require('haylen.math')
local ui = require('haylen.ui')

local SpriteTest = require('categories.sprites.sprite-test')
local Terrain = require('categories.sprites.terrain')

local Bullets = haylen.class('Bullets', SpriteTest)

Bullets.patterns = {{id = 'spiral', text = 'Spiral'}, {id = 'rings', text = 'Rings'}, {id = 'aimed', text = 'Aimed'}}
Bullets.capacity = 5000
Bullets.cell = 64
-- Each kind of bullet: the colors of its glow from the white core out to the halo, and its size, which the aimed needles stretch along their flight.
Bullets.kinds = {
    {rim = '#FFFFC94A', halo = '#FFFF8A1E', width = 30, height = 30},
    {rim = '#FF8CF0FF', halo = '#FF1EA8FF', width = 28, height = 28},
    {rim = '#FFFF9CDC', halo = '#FFFF2E9A', width = 28, height = 28},
    {rim = '#FFE2B8FF', halo = '#FF9A3CFF', width = 46, height = 18},
}
-- The color of the light the crystal throws on the clearing in each pattern.
Bullets.lights = {spiral = '#FFFF9A2E', rings = '#FF4AC8FF', aimed = '#FFA855FF'}
Bullets.night = '#FF5A6A9C'
Bullets.turretSize = 230

Bullets.code = [[
image:fillGradient({kind = 'radial', stops = {{0, '#FFFFFFFF'}, {0.32, rim}, {1, clear}}}, cell)
local bullets = collections.newPool({create = newBullet, reset = aim, capacity = 5000, prewarm = 500})
bullets:acquire(x, y, angle, speed, kind)
bullets:release(bullet)  -- Back to the pool, never collected.
bullets:each(move)
graphics2d.drawBatch(glows, sprites, {blend = 'additive'})]]

-- Paints the glow of every kind of bullet into one texture, a white core inside its color, so all of them draw in one batch.
function Bullets.glows()
    local cell = Bullets.cell
    local image = graphics.newImage(cell * #Bullets.kinds, cell)
    local sources = {}
    for index, kind in ipairs(Bullets.kinds) do
        local halo = m.color(kind.halo)
        local faint, clear = m.color(halo.r, halo.g, halo.b, 0.35), m.color(halo.r, halo.g, halo.b, 0)
        image:fillGradient({kind = 'radial', stops = {{0, '#FFFFFFFF'}, {0.16, '#FFFFFFFF'}, {0.32, kind.rim}, {0.5, halo}, {0.68, faint}, {1, clear}}}, {(index - 1) * cell, 0, cell, cell})
        sources[index] = {(index - 1) * cell, 0, cell, cell}
    end
    return graphics.newTexture(image, {filter = 'linear'}), sources
end

-- The light of the crystal on the ground and the darkness at the edges of the clearing, as two gradients.
function Bullets.shades()
    local light = graphics.newImage(128, 128)
    light:fillGradient({kind = 'radial', stops = {{0, '#FFFFFFFF'}, {0.45, '#66FFFFFF'}, {1, '#00FFFFFF'}}})
    local vignette = graphics.newImage(128, 128)
    vignette:fillGradient({kind = 'radial', radius = 0.72, stops = {{0, '#00000000'}, {0.6, '#00000000'}, {1, '#B0080A14'}}})
    return graphics.newTexture(light, {filter = 'linear'}), graphics.newTexture(vignette, {filter = 'linear'})
end

function Bullets:enter()
    self.glows, self.sources = Bullets.glows()
    self.light, self.vignette = Bullets.shades()
    self.turret = SpriteTest.texture('images/turret.png')
    self.lightColors = {}
    for pattern, color in pairs(Bullets.lights) do
        self.lightColors[pattern] = m.color(color)
    end
    self.flashColor = m.color(1, 1, 1, 0)
    self.pattern, self.rate = 'spiral', 1
    self.angle, self.cooldown, self.time, self.flash = 0, 0, 0, 0
    self.sprites = {}
    self.pool = collections.newPool({
        create = function()
            return {vx = 0, vy = 0, sprite = {x = 0, y = 0, width = 0, height = 0, rotation = 0, source = self.sources[1]}}
        end,
        reset = function(bullet, x, y, angle, speed, kind)
            bullet.vx, bullet.vy = math.cos(angle) * speed, math.sin(angle) * speed
            local sprite, size = bullet.sprite, Bullets.kinds[kind]
            sprite.x, sprite.y, sprite.rotation, sprite.source = x, y, angle, self.sources[kind]
            sprite.width, sprite.height = size.width, size.height
        end,
        capacity = Bullets.capacity,
        prewarm = 500,
    })
    -- The walk over the pool is one function made once, so moving the bullets allocates nothing.
    self.move = function(bullet)
        local sprite = bullet.sprite
        sprite.x, sprite.y = sprite.x + bullet.vx * self.dt, sprite.y + bullet.vy * self.dt
        if sprite.x < -30 or sprite.y < -30 or sprite.x > self.area.width + 30 or sprite.y > self.area.height + 30 then
            self.pool:release(bullet)
        else
            self.count = self.count + 1
            self.sprites[self.count] = sprite
        end
    end
    self:frame{
        code = Bullets.code,
        hint = 'In the aimed pattern the turret shoots at the pointer, or at a mark that circles it.',
        controls = {
            ui.formField{label = 'Pattern', ui.segmentedControl{id = 'pattern', items = Bullets.patterns, selected = 'spiral', onChange = function(event) self.pattern = event.value end}},
            ui.formField{label = 'Fire rate', ui.slider{id = 'rate', min = 0.5, max = 4, value = 1, step = 0.5, decimals = 1, showValue = true, onChange = function(event) self.rate = event.value end}},
            ui.button{id = 'clear', text = 'Release every bullet', onClick = function() self.pool:releaseAll() end},
        },
        focus = 'pattern',
    }
end

-- Bakes the clearing once the stage has a size: grass with a round patch of dirt under the turret.
function Bullets:resize(area)
    local size = 96
    local columns, rows = math.ceil(area.width / size), math.ceil(area.height / size)
    local centerX, centerY = area.width / 2 / size, area.height / 2 / size
    self.grass, self.edges = Terrain.bake(columns, rows, size, 0, 0, function(column, row)
        local dx, dy = (column - centerX) / 2.6, (row - centerY) / 1.9
        return dx * dx + dy * dy < 1 and Terrain.path or Terrain.grass
    end)
end

-- The crystal floats above the socket of the turret base and bobs, and every bullet leaves from its middle.
function Bullets:muzzle()
    local area = self.area
    return area.width / 2, area.height / 2 - Bullets.turretSize * 0.32 + math.sin(self.time * 2.4) * 5
end

-- The aimed pattern shoots at the pointer while it is over the stage, and at a mark that circles the turret otherwise.
function Bullets:target()
    local x, y = self:pointer()
    if self.area:contains({x, y}) then
        return x, y
    end
    local radius = math.min(self.area.width, self.area.height) * 0.38
    return self.area.width / 2 + math.cos(self.time * 0.7) * radius, self.area.height / 2 + math.sin(self.time * 0.7) * radius
end

function Bullets:fire(x, y, angle, speed, kind)
    self.pool:acquire(x, y, angle, speed, kind)
end

-- Fires one volley of the current pattern from the crystal and returns the time until the next one.
function Bullets:volley(x, y)
    self.flash = 1
    if self.pattern == 'spiral' then
        self.angle = self.angle + 0.21
        for arm = 0, 4 do
            self:fire(x, y, self.angle + arm * m.tau / 5, 260, 1)
        end
        return 0.05
    elseif self.pattern == 'rings' then
        self.angle = self.angle + 0.13
        for index = 0, 35 do
            self:fire(x, y, self.angle + index * m.tau / 36, 200, index % 2 == 0 and 2 or 3)
        end
        return 0.3
    end
    local targetX, targetY = self:target()
    local aim = math.atan(targetY - y, targetX - x)
    for index = -3, 3 do
        self:fire(x, y, aim + index * 0.09, 420, 4)
    end
    return 0.12
end

function Bullets:update(dt)
    Bullets.super.update(self, dt)
    if not self.area then
        return
    end
    self.time = self.time + dt
    self.flash = math.max(0, self.flash - dt * 6)
    local x, y = self:muzzle()
    self.cooldown = self.cooldown - dt * self.rate
    while self.cooldown <= 0 do
        self.cooldown = self.cooldown + self:volley(x, y)
    end

    self.dt, self.count = dt, 0
    self.pool:each(self.move)
    for index = self.count + 1, #self.sprites do
        self.sprites[index] = nil
    end
    local frame = debugging.frame()
    local pool = self.pool
    self:status(string.format('%s bullets in flight and %s waiting in the pool of %s. %.0f frames per second, %.2f ms per frame.', localization.formatNumber(pool.active), localization.formatNumber(pool.idle), localization.formatNumber(pool.capacity), frame.fps, frame.milliseconds))
end

function Bullets:drawTurret(x, y, light)
    local size, cell = Bullets.turretSize, self.turret.height
    local baseY = self.area.height / 2
    self.flashColor.a = self.flash * 0.45
    graphics2d.drawEllipse(x, baseY + size * 0.3, size * 0.46, size * 0.14, '#70000000', {layer = 1})
    graphics2d.draw(self.turret, x, baseY, {source = {0, 0, cell, cell}, width = size, height = size, color = '#FFC8CCE6', layer = 2})
    -- The glow behind the crystal stays faint, so the crystal keeps its colors, and flares with every volley.
    light.a = 0.2 + self.flash * 0.3
    graphics2d.draw(self.light, x, y, {width = 170, height = 170, color = light, blend = 'additive', layer = 3})
    graphics2d.draw(self.turret, x, y, {source = {cell, 0, cell, cell}, width = size * 0.78, height = size * 0.78, flash = self.flashColor, layer = 8})
end

function Bullets:draw(area)
    graphics2d.drawStatic(self.grass, 0, 0, {layer = -10})
    graphics2d.drawStatic(self.edges, 0, 0, {layer = -9})
    graphics2d.drawRect(graphics2d.canvasBounds(), Bullets.night, {layer = -8, blend = 'multiply'})

    local x, y = self:muzzle()
    local light = self.lightColors[self.pattern]
    light.a = 0.55 + self.flash * 0.25
    graphics2d.draw(self.light, area.width / 2, area.height / 2, {width = 640, height = 440, color = light, blend = 'additive', layer = -7})
    self:drawTurret(x, y, light)
    if self.pattern == 'aimed' then
        local targetX, targetY = self:target()
        graphics2d.drawRing(targetX, targetY, 18, 3, '#C0E2B8FF', {layer = 5})
        graphics2d.drawCircle(targetX, targetY, 4, '#FFE2B8FF', {layer = 5})
    end
    graphics2d.drawBatch(self.glows, self.sprites, {layer = 6, blend = 'additive'})
    graphics2d.draw(self.vignette, 0, 0, {width = area.width, height = area.height, pivotX = 0, pivotY = 0, layer = 7})
end

return Bullets
