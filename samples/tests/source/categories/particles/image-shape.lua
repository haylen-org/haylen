-- Image shapes: `assets.load` with the type `imageShape` reads the visible pixels of a sprite, and emitters of the `image` shape spawn on them, scaled to `shapeSize`, so sparkles run over the hero and, with `colorFromImage`, the hero breaks apart into pieces in the colors of its own pixels.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local ImageShapes = haylen.class('ImageShapes', ParticleTest)

ImageShapes.code = [[
local shape = assets.load('sprites/images/hero.png', 'imageShape', {alphaThreshold = 0.5})
local sparkles = particles2d.newEmitter({texture = sparkle, shape = 'image', shapeImage = shape})
sparkles:configure({shapeSize = {384, 384}, blend = 'additive'})
local pieces = particles2d.newEmitter({texture = pixel, shape = 'image', shapeImage = shape})
pieces:configure({colorFromImage = true, gravity = {0, 900}})  -- In their own colors.
pieces:burst(1600)]]

ImageShapes.path = 'sprites/images/hero.png'
ImageShapes.size = 384
ImageShapes.y = 100
ImageShapes.floor = 290
ImageShapes.away = 2.4
ImageShapes.idle = 3.2

function ImageShapes:init(entry)
    ImageShapes.super.init(self, entry)
    self.pieceCount = 1600
    self.ownColors = true
    self.shape = assets.load(ImageShapes.path, 'imageShape', {alphaThreshold = 0.5})
    self.hero = assets.texture(ImageShapes.path)
    self.style = {width = ImageShapes.size, height = ImageShapes.size, layer = 1}
    self.shown = true
    self.clock = 0

    local size = {ImageShapes.size, ImageShapes.size}
    self.sparkles = particles2d.newEmitter(ParticleTest.look('sparkle', {rate = 30, lifetime = {0.5, 0.9}, speed = {0, 15}, startSize = {22, 34}, endSize = 6, colors = {'#FFFFFFFF', '#FFFFF0A0', '#00FFC060'}, shape = 'image', shapeImage = self.shape, shapeSize = size, blend = 'additive', layer = 3, seed = 261}))
    self.pieces = particles2d.newEmitter(ParticleTest.look('pixel-square', {rate = 0, lifetime = {1.6, 2.4}, speed = {80, 420}, directionMode = 'outward', spread = 0.8, gravity = {0, 900}, damping = 0.6, startSize = {8, 12}, endSize = {8, 12}, spin = {-6, 6}, colors = {'#FFFFFFFF', '#FFFFFFFF', '#00FFFFFF'}, colorTimes = {0, 0.75, 1}, shape = 'image', shapeImage = self.shape, shapeSize = size, colorFromImage = self.ownColors, collision = {type = 'floor', y = ImageShapes.floor - ImageShapes.y, bounce = 0.3, friction = 0.4}, maxParticles = 4096, layer = 2, seed = 262}))
    for _, emitter in ipairs({self.sparkles, self.pieces}) do
        emitter.x, emitter.y = 0, ImageShapes.y
    end
    self.emitters = {self.sparkles, self.pieces}
end

function ImageShapes:breakApart()
    if not self.shown then
        return
    end
    self.shown = false
    self.clock = 0
    self.sparkles.emitting = false
    self.pieces:burst(self.pieceCount)
end

function ImageShapes:enter()
    self:frame{
        hint = 'A click, a tap, E, Enter, the south button or the button of the panel breaks the hero apart, and it comes back after a moment. Without input it breaks by itself.',
        code = ImageShapes.code,
        cursor = true,
        controls = {
            ui.button{id = 'break', text = 'Break apart', onClick = function()
                self:breakApart()
            end},
            ui.formField{label = 'Pieces', ui.slider{id = 'pieces', min = 200, max = 4000, step = 100, value = self.pieceCount, showValue = true, decimals = 0, onChange = function(event)
                self.pieceCount = math.floor(event.value)
            end}},
            ui.toggle{id = 'ownColors', text = 'Colors of the image', checked = self.ownColors, onChange = function(event)
                self.ownColors = event.checked
                self.pieces:configure({colorFromImage = event.checked})
            end},
        },
    }
end

function ImageShapes:update(dt)
    ImageShapes.super.update(self, dt)
    self.clock = self.clock + dt
    if self:pressed() or (self.shown and self.clock > ImageShapes.idle) then
        self:breakApart()
    elseif not self.shown and self.clock > ImageShapes.away then
        self.shown = true
        self.clock = 0
        self.sparkles.emitting = true
        self.sparkles:burst(90)
    end
    ParticleTest.updateAll(self.emitters, dt)
    self:report('The hero has %d visible pixels of %d by %d, and %d sparkles and %d pieces spawn on them.', self.shape.count, self.shape.width, self.shape.height, self.sparkles.count, self.pieces.count)
end

function ImageShapes:draw(area)
    ParticleTest.backdrop({0.08, 0.06, 0.14}, {0.2, 0.14, 0.2})
    ParticleTest.paveFloor('stone', ImageShapes.floor, 140, '#FF7A7090')
    if self.shown then
        graphics2d.draw(self.hero, 0, ImageShapes.y, self.style)
    end
    ParticleTest.drawAll(self.emitters)
end

return ImageShapes
