-- Sub-emitters: every entry of `subEmitters` spawns the particles of another effect where a particle of its emitter dies, hits something or, at a rate, while it lives, with the share `inheritVelocity` of its velocity and, with `inheritColor`, its color. The effect of a sub-emitter may have sub-emitters of its own, like the stars of a firework that crackle when they die.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local SubEmitters = haylen.class('SubEmitters', ParticleTest)

SubEmitters.ground = 400
SubEmitters.floor = {-1000, 400, 2000, 200}
SubEmitters.order = {layer = 1}

function SubEmitters:init(entry)
    SubEmitters.super.init(self, entry)
    self.inheritColor = true
    self.cometDrag = 0.2
    local dot, spark = ParticleTest.library('soft_dot'), ParticleTest.library('hard_dot')

    local splash = {texture = spark, lifetime = {0.25, 0.45}, speed = {80, 220}, direction = -1.5708, spread = 2.2, gravity = {0, 900}, startSize = {5, 8}, endSize = 2, colors = {'#E0C0D8F8', '#00C0D8F8'}, maxParticles = 512, layer = 3}
    self.rain = particles2d.newEmitter({texture = ParticleTest.library('rain_streak'), rate = 70, lifetime = 3, speed = {900, 1050}, direction = 1.5708, spread = 0.02, startSize = 40, endSize = 40, colors = {'#B0B0C8F0'}, shape = 'rectangle', shapeSize = {230, 4}, collision = {type = 'floor', y = SubEmitters.ground + 500, result = 'die'}, subEmitters = {{effect = splash, trigger = 'death', count = {3, 5}}}, maxParticles = 256, layer = 2, seed = 291})
    self.rain.x, self.rain.y = -680, -500

    self.crackle = {texture = ParticleTest.library('star_4point'), lifetime = {0.15, 0.35}, speed = {20, 120}, spread = m.tau, startSize = {10, 18}, endSize = 0, colors = {'#FFFFFFFF', '#00FFFFFF'}, blend = 'additive', maxParticles = 512, layer = 5}
    self.stars = {texture = dot, lifetime = {0.9, 1.3}, speed = {160, 380}, spread = m.tau, gravity = {0, 220}, damping = 1.2, startSize = {14, 20}, endSize = 4, colors = {'#FFFFFFFF', '#FFFFFFFF', '#00FFFFFF'}, blend = 'additive', subEmitters = {{effect = self.crackle, trigger = 'death', count = {1, 3}, probability = 0.6}}, maxParticles = 1024, layer = 4}
    self.rockets = particles2d.newEmitter({texture = dot, rate = 1.2, lifetime = {1, 1.2}, speed = {720, 840}, direction = -1.5708, spread = 0.35, gravity = {0, 420}, startSize = 16, endSize = 12, colors = {'#FFFFFFFF'}, tints = {'#FFFF5070', '#FFFFD040', '#FF50E080', '#FF50A0FF', '#FFD070FF'}, blend = 'additive', subEmitters = self:rocketSubEmitters(), maxParticles = 16, layer = 3, seed = 292})
    self.rockets.x, self.rockets.y = 0, SubEmitters.ground

    local dust = {texture = ParticleTest.library('smoke_puff'), lifetime = {0.6, 1}, speed = {30, 90}, direction = -1.5708, spread = 2.4, startSize = {20, 30}, endSize = {60, 90}, rotation = {0, m.tau}, colors = {'#A0B09070', '#00806040'}, maxParticles = 256, layer = 2}
    self.debris = particles2d.newEmitter({texture = ParticleTest.library('debris_sheet'), frameGrid = {columns = 4, rows = 1}, frameMode = 'random', rate = 0, bursts = {{time = 0, count = {10, 14}}}, duration = 2.2, loop = true, lifetime = {2, 2.6}, speed = {350, 650}, direction = -1.5708, spread = 1.2, gravity = {0, 1100}, startSize = {24, 40}, endSizeScale = 1, rotation = {0, m.tau}, spin = {-6, 6}, colors = {'#FFFFFFFF', '#FFFFFFFF', '#00FFFFFF'}, colorTimes = {0, 0.85, 1}, collision = {type = 'floor', y = 60, bounce = 0.35, friction = 0.5, lifeLoss = 0.1}, subEmitters = {{effect = dust, trigger = 'collision', count = {1, 2}, inheritVelocity = 0.1}}, maxParticles = 128, layer = 3, seed = 293})
    self.debris.x, self.debris.y = 680, SubEmitters.ground - 60

    self.comets = particles2d.newEmitter({texture = dot, rate = 0.5, lifetime = 4, speed = {460, 520}, direction = 0.1, spread = 0.1, startSize = 28, endSize = 22, colors = {'#FFFFF0D0'}, blend = 'additive', subEmitters = self:cometSubEmitters(), maxParticles = 8, layer = 3, seed = 294})
    self.comets.x, self.comets.y = -980, -440

    self.emitters = {self.rain, self.rockets, self.debris, self.comets}
end

-- The rockets burst into stars when they die, and the stars take the tint of their rocket while `inheritColor` is on.
function SubEmitters:rocketSubEmitters()
    return {{effect = self.stars, trigger = 'death', count = {40, 60}, inheritColor = self.inheritColor}}
end

-- The comets leave dust while they live, which keeps the share of their velocity of the slider.
function SubEmitters:cometSubEmitters()
    return {{effect = {texture = ParticleTest.library('sparkle_sheet'), frameGrid = {columns = 4, rows = 1}, lifetime = {0.6, 1}, speed = {0, 30}, spread = m.tau, startSize = {12, 20}, endSize = 0, colors = {'#FFFFE0A0', '#00FF8040'}, blend = 'additive', maxParticles = 1024, layer = 2}, trigger = 'alive', rate = 60, inheritVelocity = self.cometDrag}}
end

function SubEmitters:enter()
    self:frame{
        hint = 'The drops splash where they die on the ground, the rockets burst into stars that crackle, the debris raises dust at every hit and the comets leave dust behind them.',
        controls = {
            ui.toggle{id = 'inheritColor', text = 'Stars take the color of their rocket', checked = self.inheritColor, onChange = function(event)
                self.inheritColor = event.checked
                self.rockets:configure({subEmitters = self:rocketSubEmitters()})
            end},
            ui.formField{label = 'Velocity the comet dust inherits', ui.slider{id = 'cometDrag', min = 0, max = 1, value = self.cometDrag, showValue = true, onChange = function(event)
                self.cometDrag = event.value
                self.comets:configure({subEmitters = self:cometSubEmitters()})
            end}},
        },
        focus = 'inheritColor',
    }
end

function SubEmitters:update(dt)
    SubEmitters.super.update(self, dt)
    ParticleTest.updateAll(self.emitters, dt)
    self:report('Drops %d   Rockets %d   Debris %d   Comets %d   Stars take the rocket color "%s"   Comet dust inherits %.2f', self.rain.count, self.rockets.count, self.debris.count, self.comets.count, self.inheritColor, self.cometDrag)
end

function SubEmitters:draw(area)
    ParticleTest.backdrop({0.02, 0.02, 0.07}, {0.08, 0.07, 0.16})
    graphics2d.drawRect(SubEmitters.floor, '#FF1E2230', SubEmitters.order)
    ParticleTest.drawAll(self.emitters)
    ParticleTest.label('Splashes at death', self.rain.x, SubEmitters.ground + 30)
    ParticleTest.label('Stars at death that crackle', self.rockets.x, SubEmitters.ground + 30)
    ParticleTest.label('Dust at every hit', self.debris.x, SubEmitters.ground + 30)
end

return SubEmitters
