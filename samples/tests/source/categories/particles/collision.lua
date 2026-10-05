-- Collision: particles hit a floor below their emitter, the walls of an area around it, or the shapes of a physics world given with `emitter:setCollisionWorld`, whose moves the emitter casts through the world in one batch. A hit bounces, sticks or kills the particle, keeping the share `bounce` of its speed into the surface and losing the share `friction` along it.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Collision = haylen.class('Collision', ParticleTest)

Collision.results = {{id = 'bounce', text = 'Bounce'}, {id = 'stick', text = 'Stick'}, {id = 'die', text = 'Die'}}
Collision.floor = 360
Collision.spout = -200
-- The emitter of the balls stays at the origin, so the area of its walls, relative to it, is also where they are drawn.
Collision.area = {-230, -230, 460, 460}
Collision.ground = {-1100, 360, 780, 200}
Collision.walls = {layer = 1}
Collision.bodies = {layer = 3}

function Collision:init(entry)
    Collision.super.init(self, entry)
    self.result = 'bounce'
    self.bounce = 0.5
    self.white = graphics.whiteTexture()

    self.sparks = particles2d.newEmitter({texture = ParticleTest.library('spark_streak'), rate = 90, lifetime = {1.8, 2.6}, speed = {300, 650}, direction = 1.5708, spread = 1.8, gravity = {0, 1200}, startSize = 12, endSize = 6, alignToVelocity = true, stretch = 0.02, colors = {'#FFFFFFE0', '#FFFFB040', '#FFFF6020', '#00A02000'}, collision = self:floorCollision(), blend = 'additive', maxParticles = 512, layer = 2, seed = 231})
    self.sparks.x, self.sparks.y = -640, Collision.spout

    self.balls = particles2d.newEmitter({texture = ParticleTest.library('hard_dot'), rate = 6, lifetime = {6, 8}, speed = {300, 600}, spread = m.tau, gravity = {0, 500}, startSize = 26, endSize = 26, colors = {'#00FFFFFF', '#FFFFFFFF', '#FFFFFFFF', '#00FFFFFF'}, colorTimes = {0, 0.05, 0.9, 1}, tints = {'#FFFF8080', '#FFFFD060', '#FF80E0A0', '#FF80B0FF', '#FFD090FF'}, collision = {type = 'bounds', area = Collision.area, bounce = 0.9}, maxParticles = 64, layer = 2, seed = 232})

    self.world = physics2d.newWorld()
    self.round = self.world:createBody({type = 'static', x = 540, y = -170})
    self.round:addCircle(70)
    self.plank = self.world:createBody({type = 'static', x = 790, y = -10, rotation = -0.35})
    self.plank:addBox(280, 24)
    self.paddle = self.world:createBody({type = 'kinematic', x = 600, y = 190, angularVelocity = 1.2})
    self.paddle:addBox(280, 22)
    self.base = self.world:createBody({type = 'static', x = 640, y = 380})
    self.base:addBox(620, 40)
    self.plankStyle = {width = 280, height = 24, rotation = -0.35, color = '#FF8A6A4A', layer = 3}
    self.paddleStyle = {width = 280, height = 22, rotation = 0, color = '#FF6A8AC0', layer = 3}
    self.baseStyle = {width = 620, height = 40, color = '#FF4A5060', layer = 3}

    self.drops = particles2d.newEmitter({texture = ParticleTest.library('droplet'), rate = 110, lifetime = 2.6, speed = {60, 120}, direction = 1.5708, spread = 0.1, gravity = {0, 1000}, startSize = 18, endSize = 14, colors = {'#FFA0D0FF', '#FFA0D0FF', '#0080B0FF'}, colorTimes = {0, 0.8, 1}, shape = 'rectangle', shapeSize = {230, 4}, collision = {type = 'world', bounce = 0.35, friction = 0.1}, maxParticles = 512, layer = 2, seed = 233})
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
    self:report('Floor result "%s" with bounce %.2f   Sparks %d   Balls %d   Drops %d   Bodies %d', self.result, self.bounce, self.sparks.count, self.balls.count, self.drops.count, self.world.bodyCount)
end

function Collision:draw(area)
    ParticleTest.backdrop({0.05, 0.06, 0.1}, {0.12, 0.12, 0.18})
    graphics2d.drawRect(Collision.ground, '#FF2A2C34', Collision.walls)
    graphics2d.drawRectOutline(Collision.area, 4, '#FF8090B0', Collision.walls)
    graphics2d.drawCircle(self.round.x, self.round.y, 70, '#FF8A6A9A', Collision.bodies)
    graphics2d.draw(self.white, self.plank.x, self.plank.y, self.plankStyle)
    self.paddleStyle.rotation = self.paddle.rotation
    graphics2d.draw(self.white, self.paddle.x, self.paddle.y, self.paddleStyle)
    graphics2d.draw(self.white, self.base.x, self.base.y, self.baseStyle)
    ParticleTest.drawAll(self.emitters)
    ParticleTest.label('Floor', -640, 400)
    ParticleTest.label('Walls of the collision type "bounds"', 0, 250)
    ParticleTest.label('Bodies of a physics world', 640, 410)
end

return Collision
