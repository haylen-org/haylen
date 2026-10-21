-- The hero of the physics tests that drive a character: the walker of the project, standing, running or in the air over the body that moves it, facing where it goes.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Hero = haylen.class('Hero')

-- The walker sheet has cells of 256 pixels in eight columns, the idle frames in the first row and the run in the second, with the feet on the ground line of the cells and the walker this many pixels tall.
Hero.cell = 256
Hero.columns = 8
Hero.groundLine = 248
Hero.tall = 228
Hero.idle = {0, 1, 2, 3}
Hero.run = {8, 9, 10, 11, 12, 13, 14, 15}
Hero.air = 10
Hero.runSpeed = 300

-- The argument `height` is how tall the hero stands in world units.
function Hero:init(height)
    self.texture = assets.texture('sprites/sheets/walker.png', {filter = 'linear'})
    self.size = Hero.cell * height / Hero.tall
    self.time = 0
    self.facing = 1
    self.state = 'idle'
    self.source = {0, 0, Hero.cell, Hero.cell}
    self.options = {source = self.source, width = self.size, height = self.size, pivotX = 0.5, pivotY = Hero.groundLine / Hero.cell, flipHorizontal = false, rotation = 0, layer = 0}
end

-- Follows the speed of the body along the ground, or the air while it is not on the ground.
function Hero:update(dt, speed, grounded)
    if math.abs(speed) > 20 then
        self.facing = speed > 0 and 1 or -1
    end
    if not grounded then
        self.state = 'air'
    elseif math.abs(speed) > 30 then
        self.state = 'run'
        self.time = self.time + dt * math.max(0.6, math.abs(speed) / Hero.runSpeed)
        return
    else
        self.state = 'idle'
    end
    self.time = self.time + dt
end

-- Draws the hero with its feet at `x`, `y`, turned by `rotation`, such as the swing of a rope.
function Hero:draw(x, y, layer, rotation)
    local frame = Hero.air
    if self.state == 'run' then
        frame = Hero.run[math.floor(self.time * 14) % #Hero.run + 1]
    elseif self.state == 'idle' then
        frame = Hero.idle[math.floor(self.time * 6) % #Hero.idle + 1]
    end
    self.source[1] = frame % Hero.columns * Hero.cell
    self.source[2] = frame // Hero.columns * Hero.cell
    local options = self.options
    options.flipHorizontal = self.facing < 0
    options.rotation = rotation or 0
    options.layer = layer or 0
    graphics2d.draw(self.texture, x, y, options)
end

return Hero
