-- Collision: particles hit a floor below their emitter, the walls of an area around it, or the shapes of a physics world given with `emitter:setCollisionWorld`, whose moves the emitter casts through the world in one batch. They touch at their edge, so the balls stay whole inside their box, and a hit bounces, sticks or kills the particle, keeping the share `bounce` of its speed into the surface and losing the share `friction` along it.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Collision = haylen.class('Collision', ParticleTest)

Collision.code = [[
sparks:configure({collision = {type = 'floor', y = 560, bounce = 0.5, result = 'bounce'}})
balls:configure({collision = {type = 'bounds', area = {-230, -230, 460, 460}, bounce = 0.9}})
-- Particles touch at their edge, half their size from the center, or at "sizeScale" of it.
local drops = particles2d.newEmitter({texture = droplet, gravity = {0, 1000}})
drops:configure({collision = {type = 'world', bounce = 0.35, friction = 0.1}})
drops:setCollisionWorld(world)  -- The shapes of a physics world.]]

Collision.results = {{id = 'bounce', text = 'Bounce'}, {id = 'stick', text = 'Stick'}, {id = 'die', text = 'Die'}}
Collision.floor = 360
Collision.spout = -200
-- The emitter of the balls stays at the origin, so the area of its walls, relative to it, is also where they are drawn.
Collision.area = {-230, -230, 460, 460}
Collision.walls = {layer = 1}
Collision.ballSize = 64

-- Paints a ball of two tones and a highlight, lit from the top left, inside a dark outline that fades over a pixel, which the tints of the balls color.
function Collision.ballImage()
    local size = Collision.ballSize
    local image = graphics.newImage(size, size)
    local center, radius = size / 2, size / 2 - 1
    for y = 0, size - 1 do
        for x = 0, size - 1 do
            local dx, dy = x + 0.5 - center, y + 0.5 - center
            local distance = math.sqrt(dx * dx + dy * dy)
            local cover = m.clamp(radius - distance, 0, 1)
            if cover > 0 then
                local light = (-dx - dy) / (radius * 1.414)
                local tone = distance > radius - 4 and 0.16 or light > 0.15 and 1 or 0.78
                if distance < radius * 0.3 and dx < 0 and dy < 0 and math.sqrt((dx + radius * 0.35) ^ 2 + (dy + radius * 0.35) ^ 2) < radius * 0.16 then
                    tone = 1.15
                end
                image:setPixel(x, y, m.color(math.min(tone, 1), math.min(tone, 1), math.min(tone * 1.05, 1), cover))
            end
        end
    end
    return image
end

function Collision:init(entry)
    Collision.super.init(self, entry)
    self.result = 'bounce'
    self.bounce = 0.5
    self.ballTexture = graphics.newTexture(Collision.ballImage(), {filter = 'linear'})
    local effects = ParticleTest.look('plank', {})
    self.plankTexture, self.plankSource = effects.texture, effects.frames[1]

    self.sparks = particles2d.newEmitter(ParticleTest.look('spark-streak', {rate = 90, lifetime = {1.8, 2.6}, speed = {300, 650}, direction = 1.5708, spread = 1.8, gravity = {0, 1200}, startSize = 12, endSize = 6, alignToVelocity = true, stretch = 0.02, colors = {'#FFFFFFE0', '#FFFFB040', '#FFFF6020', '#00A02000'}, collision = self:floorCollision(), blend = 'additive', maxParticles = 512, layer = 2, seed = 231}))
    self.sparks.x, self.sparks.y = -640, Collision.spout

    self.balls = particles2d.newEmitter({texture = self.ballTexture, rate = 6, lifetime = {6, 8}, speed = {300, 600}, spread = m.tau, gravity = {0, 500}, startSize = 44, endSize = 44, colors = {'#00FFFFFF', '#FFFFFFFF', '#FFFFFFFF', '#00FFFFFF'}, colorTimes = {0, 0.05, 0.9, 1}, tints = {'#FFFF8080', '#FFFFD060', '#FF80E0A0', '#FF80B0FF', '#FFD090FF'}, collision = {type = 'bounds', area = Collision.area, bounce = 0.9}, maxParticles = 64, layer = 2, seed = 232})

    self.world = physics2d.newWorld()
    self.round = self.world:createBody({type = 'static', x = 540, y = -170})
    self.round:addCircle(70)
    self.plank = self.world:createBody({type = 'static', x = 790, y = -10, rotation = -0.35})
    self.plank:addBox(280, 24)
    self.paddle = self.world:createBody({type = 'kinematic', x = 600, y = 190, angularVelocity = 1.2})
    self.paddle:addBox(280, 22)
    self.base = self.world:createBody({type = 'static', x = 500, y = Collision.floor + 100})
    self.base:addBox(1700, 200)

    self.drops = particles2d.newEmitter(ParticleTest.look('droplet', {rate = 110, lifetime = 2.6, speed = {60, 120}, direction = 1.5708, spread = 0.1, gravity = {0, 1000}, startSize = 18, endSize = 14, colors = {'#FFA0D0FF', '#FFA0D0FF', '#0080B0FF'}, colorTimes = {0, 0.8, 1}, shape = 'rectangle', shapeSize = {230, 4}, collision = {type = 'world', bounce = 0.35, friction = 0.1}, maxParticles = 512, layer = 2, seed = 233}))
    self.drops.x, self.drops.y = 640, -480
    self.drops:setCollisionWorld(self.world)

    self.emitters = {self.sparks, self.balls, self.drops}
end

-- Returns the floor of the sparks, whose height is relative to their emitter, with the result and the bounce of the controls.
function Collision:floorCollision()
    return {type = 'floor', y = Collision.floor - Collision.spout, bounce = self.bounce, friction = 0.2, result = self.result}
end

function Collision:enter()
    self:frame{
        hint = 'The sparks hit a floor, the balls stay inside the walls of the box and the drops hit the circle, the plank, the turning paddle and the ground of a physics world.',
        code = Collision.code,
        controls = {
            ui.formField{label = 'What a spark does on the floor', ui.radioGroup{id = 'result', items = Collision.results, selected = self.result, onChange = function(event)
                self.result = event.value
                self.sparks:configure({collision = self:floorCollision()})
            end}},
            ui.formField{label = 'Bounce of the sparks', ui.slider{id = 'bounce', min = 0, max = 1.2, value = self.bounce, showValue = true, onChange = function(event)
                self.bounce = event.value
                self.sparks:configure({collision = self:floorCollision()})
            end}},
        },
        focus = 'result',
    }
end

function Collision:fixedUpdate(step)
    self.world:step(step)
end

function Collision:update(dt)
    Collision.super.update(self, dt)
    ParticleTest.updateAll(self.emitters, dt)
    self:report('Sparks %s on the floor with a bounce of %.2f, %d balls bounce in their box, and %d drops hit %d bodies.', self.result == 'bounce' and 'bounce' or self.result == 'stick' and 'stick' or 'die', self.bounce, self.balls.count, self.drops.count, self.world.bodyCount)
end

-- Draws a body of a box shape as a wooden plank with a dark outline, turned with the body.
function Collision.drawPlank(texture, source, body, width, height)
    graphics2d.drawShape({body.x - width / 2 - 4, body.y - height / 2 - 4, width + 8, height + 8}, {radius = 6, rotation = body.rotation, color = '#FF1E1B33', layer = 3})
    graphics2d.draw(texture, body.x, body.y, {source = source, width = width, height = height, rotation = body.rotation, layer = 3})
end

function Collision:draw(area)
    ParticleTest.backdrop({0.07, 0.08, 0.13}, {0.14, 0.14, 0.2})
    ParticleTest.paveFloor('stone', Collision.floor, 140, '#FF8A8C9C')
    graphics2d.drawShape(Collision.area, {radius = 10, color = '#301E1B33', borderWidth = 8, borderColor = '#FF9AA6C8', layer = 1})
    graphics2d.drawCircle(self.round.x, self.round.y, 70, '#FF1E1B33', {layer = 3})
    graphics2d.drawCircle(self.round.x, self.round.y, 64, '#FF9A7AB0', {layer = 3})
    graphics2d.drawCircle(self.round.x - 14, self.round.y - 14, 40, '#FFB496C8', {layer = 3})
    Collision.drawPlank(self.plankTexture, self.plankSource, self.plank, 280, 24)
    Collision.drawPlank(self.plankTexture, self.plankSource, self.paddle, 280, 22)
    ParticleTest.drawAll(self.emitters)
    ParticleTest.label('Floor', -640, Collision.floor + 30, 'type = floor')
    ParticleTest.label('Walls', 0, Collision.floor + 30, 'type = bounds')
    ParticleTest.label('Bodies', 640, Collision.floor + 30, 'type = world')
end

return Collision
