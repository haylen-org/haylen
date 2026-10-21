-- Size, speed and spin curves: `sizeCurve` eases the size from the start size to the end size and may pass it on the way, `endSizeScale = 1` keeps the size each particle was born with, `speedCurve` eases the speed of particles down to nothing across their life and `spinCurve` does the same for their spin.
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Curves = haylen.class('Curves', ParticleTest)

Curves.code = [[
pops:configure({sizeCurve = {curve = 'backOut', overshoot = 3}})  -- Grows past its end size.
local stars = particles2d.newEmitter({texture = star, startSize = {12, 70}})
stars:configure({endSizeScale = 1})  -- Each star keeps its own size.
dust:configure({speedCurve = 'expoOut'})  -- The dust stops early.
coins:configure({spinCurve = 'quadOut'})  -- The coins spin down.
coins:configure({spinCurve = false})  -- And keep spinning without it.]]

Curves.sizeCurves = {
    linear = 'linear',
    quadOut = 'quadOut',
    backOut = {curve = 'backOut', overshoot = 3},
    elasticOut = {curve = 'elasticOut', amplitude = 1, period = 0.35},
    points = {points = {0, 1.45, 0.85, 1}},
}
Curves.sizeItems = {{id = 'linear', text = 'Linear'}, {id = 'quadOut', text = 'Quad out'}, {id = 'backOut', text = 'Back out'}, {id = 'elasticOut', text = 'Elastic out'}, {id = 'points', text = 'Points that pop'}}
Curves.stop = 'expoOut'
Curves.spinDown = 'quadOut'
Curves.ground = 380

function Curves:init(entry)
    Curves.super.init(self, entry)
    self.sizeCurve = 'backOut'
    self.dustStops = true
    self.coinsSpinDown = true

    self.pops = particles2d.newEmitter(ParticleTest.look('bubble', {rate = 3, lifetime = 1.6, speed = 0, startSize = 0, endSize = 130, sizeCurve = Curves.sizeCurves[self.sizeCurve], colors = {'#FFFFFFFF', '#FFFFFFFF', '#00FFFFFF'}, colorTimes = {0, 0.75, 1}, shape = 'circle', shapeSize = {110, 0}, layer = 2, seed = 201}))
    self.pops.x, self.pops.y = -800, 160

    local kept = ParticleTest.look('star-5point', {rate = 5, lifetime = 3, speed = {120, 140}, spread = 0.15, startSize = {12, 70}, colors = {'#00FFD860', '#FFFFD860', '#FFFFD860', '#00FFD860'}, colorTimes = {0, 0.1, 0.85, 1}, layer = 2})
    kept.endSize, kept.seed = 40, 202
    self.converge = particles2d.newEmitter(kept)
    kept.endSize, kept.endSizeScale, kept.seed = nil, 1, 203
    self.keep = particles2d.newEmitter(kept)
    self.converge.x, self.converge.y = -420, Curves.ground - 40
    self.keep.x, self.keep.y = -60, Curves.ground - 40

    self.dust = particles2d.newEmitter(ParticleTest.look('puff', {frameMode = 'random', rate = 22, lifetime = {1.4, 2}, speed = {320, 520}, spread = 2.2, speedCurve = Curves.stop, startSize = {40, 60}, endSize = {110, 150}, rotation = {0, m.tau}, spin = {-1, 1}, colors = {'#00F0D8B8', '#E0E8D0B0', '#00D8C0A0'}, layer = 2, seed = 204}))
    self.dust.x, self.dust.y = 300, Curves.ground - 10

    self.coins = particles2d.newEmitter(ParticleTest.look('coin-1', {rate = 3, lifetime = 2.6, speed = {760, 860}, spread = 0.3, gravity = {0, 640}, startSize = 64, endSize = 64, rotation = {0, m.tau}, spin = {12, 18}, spinCurve = Curves.spinDown, colors = {'#FFFFFFFF', '#FFFFFFFF', '#00FFFFFF'}, colorTimes = {0, 0.85, 1}, layer = 3, seed = 205}))
    self.coins.x, self.coins.y = 820, Curves.ground - 10

    self.emitters = {self.pops, self.converge, self.keep, self.dust, self.coins}
    self.labels = {
        {text = 'Pops', option = 'sizeCurve', emitter = self.pops},
        {text = 'All end at 40', option = 'endSize = 40', emitter = self.converge},
        {text = 'Keep their size', option = 'endSizeScale = 1', emitter = self.keep},
        {text = 'Dust stops', option = 'speedCurve', emitter = self.dust},
        {text = 'Coins spin down', option = 'spinCurve', emitter = self.coins},
    }
end

function Curves:enter()
    self:frame{
        hint = 'The pops grow from nothing to 130 units along the picked curve. The stars are born from 12 to 70 units wide and either all end at 40 or keep their own size. The dust stops early along its curve, and the coins spin down.',
        code = Curves.code,
        controls = {
            ui.formField{label = 'Size curve of the pops', ui.radioGroup{id = 'sizeCurve', items = Curves.sizeItems, selected = self.sizeCurve, onChange = function(event)
                self.sizeCurve = event.value
                self.pops:configure({sizeCurve = Curves.sizeCurves[event.value]})
            end}},
            ui.toggle{id = 'dust', text = 'Speed curve on the dust', checked = self.dustStops, onChange = function(event)
                self.dustStops = event.checked
                self.dust:configure({speedCurve = event.checked and Curves.stop or false})
            end},
            ui.toggle{id = 'coins', text = 'Spin curve on the coins', checked = self.coinsSpinDown, onChange = function(event)
                self.coinsSpinDown = event.checked
                self.coins:configure({spinCurve = event.checked and Curves.spinDown or false})
            end},
        },
        focus = 'sizeCurve',
    }
end

function Curves:update(dt)
    Curves.super.update(self, dt)
    local count = ParticleTest.updateAll(self.emitters, dt)
    local dust = self.dustStops and 'stops early' or 'keeps its speed'
    local coins = self.coinsSpinDown and 'spin down' or 'keep spinning'
    self:report('The pops grow along "%s", the dust %s and the coins %s, with %d particles.', self.sizeCurve, dust, coins, count)
end

function Curves:draw(area)
    ParticleTest.backdrop({0.05, 0.07, 0.12}, {0.16, 0.12, 0.1})
    ParticleTest.paveFloor('dirt', Curves.ground, 140, '#FF8A7868')
    ParticleTest.drawAll(self.emitters)
    for _, label in ipairs(self.labels) do
        ParticleTest.label(label.text, label.emitter.x, Curves.ground + 24, label.option)
    end
end

return Curves
