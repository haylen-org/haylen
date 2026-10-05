-- Debug drawings: crates drawn as textured sprites fall on a ground under a pendulum and a springy pair of balls, while the drawings of "haylen.debug" switch on and off. The drawing "physics" outlines every body, shape, joint and contact of the world, "bounds" outlines every sprite with the path of its texture, and a drawer of the test adds its own drawing, all over the canvas they belong to. The full statistics show the debug overlay with the scenes of the stack and a checkbox for every drawing.
local assets = require('haylen.assets')
local debugging = require('haylen.debug')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local PhysicsTest = require('categories.physics.physics-test')
local Results = require('harness.results')

local DebugDrawings = haylen.class('DebugDrawings', PhysicsTest)

DebugDrawings.drawer = 'centers'
DebugDrawings.crates = 14
DebugDrawings.modes = {{id = 'off', text = 'Off'}, {id = 'compact', text = 'Compact'}, {id = 'full', text = 'Full'}}
DebugDrawings.code = [[
debugging.setDrawing('physics', true)  -- Or press F4, or list it in "debug.drawings" of "app.json".
debugging.addDrawer('centers', function(kind) graphics2d.drawCircle(x, y, 6, '#FFFF4080') end, {owner = self})
debugging.setStatsMode('full')  -- The overlay lists the scenes and the drawings.]]

function DebugDrawings:enter()
    self.crate = assets.texture('sprites/images/rock.png', {filter = 'linear'})
    self.hero = assets.texture('sprites/images/hero.png', {filter = 'nearest'})
    self.results = Results(self.entry.code)
    self.drawings = debugging.drawings()
    self.mode = debugging.statsMode()
    debugging.addDrawer(DebugDrawings.drawer, function(kind)
        if kind ~= 'world' then
            return
        end
        for _, body in ipairs(self.bodies) do
            graphics2d.drawCircle(body.x, body.y, 6, '#FFFF4080', {layer = 30})
        end
    end, {owner = self})
    self:frame{
        hint = 'Drag any body. F4 switches every drawing on desktops, and F3 cycles the statistics.',
        controls = {
            ui.checkbox{id = 'physics', text = 'Drawing "physics"', checked = debugging.drawing('physics'), onChange = function(event) debugging.setDrawing('physics', event.checked) end},
            ui.checkbox{id = 'bounds', text = 'Drawing "bounds"', checked = debugging.drawing('bounds'), onChange = function(event) debugging.setDrawing('bounds', event.checked) end},
            ui.checkbox{id = 'centers', text = 'Drawing "centers" of the test', checked = false, onChange = function(event) debugging.setDrawing(DebugDrawings.drawer, event.checked) end},
            ui.formField{label = 'Statistics', ui.segmentedControl{id = 'stats', items = DebugDrawings.modes, selected = self.mode, onChange = function(event) debugging.setStatsMode(event.value) end}},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'physics',
    }
    self:build()
    self:check()
end

-- The drawings the test switched stay as the app had them once it leaves.
function DebugDrawings:exit()
    for _, name in ipairs(debugging.drawingNames()) do
        local wanted = false
        for _, kept in ipairs(self.drawings) do
            wanted = wanted or kept == name
        end
        debugging.setDrawing(name, wanted)
    end
    debugging.setStatsMode(self.mode)
    DebugDrawings.super.exit(self)
end

function DebugDrawings:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.bodies = {}
    local random = m.random(5)

    local ground = self.world:createBody({type = 'static', x = 0, y = 400})
    ground:addBox(1500, 40)
    for index = 1, DebugDrawings.crates do
        local body = self.world:createBody({x = random:range(-500, 300), y = random:range(-420, 100), rotation = random:range(0, math.pi)})
        body:addBox(56, 56, {density = 1, friction = 0.6})
        self.bodies[#self.bodies + 1] = body
    end

    local pivot = self.world:createBody({type = 'static', x = 520, y = -380})
    local bob = self.world:createBody({x = 700, y = -260})
    bob:addCircle(36, {density = 2})
    self.world:createJoint('revolute', pivot, bob, {ax = 520, ay = -380})
    self.bob = bob

    local left = self.world:createBody({x = 460, y = 120})
    left:addCircle(28)
    local right = self.world:createBody({x = 640, y = 120})
    right:addCircle(28)
    self.world:createJoint('distance', left, right, {ax = 460, ay = 120, bx = 640, by = 120, enableSpring = true, hertz = 2, dampingRatio = 0.2})
    self.balls = {left, right}
end

-- Checks the names of the drawings and that switching one on and off reads back.
function DebugDrawings:check()
    local names = '"' .. table.concat(debugging.drawingNames(), '", "') .. '"'
    local listed = names:find('physics', 1, true) and names:find('bounds', 1, true) and names:find(DebugDrawings.drawer, 1, true)
    self.results:set('names', listed and 'pass' or 'fail', 'Drawing names', string.format('The engine and the test name the drawings %s.', names))
    local before = debugging.drawing(DebugDrawings.drawer)
    debugging.setDrawing(DebugDrawings.drawer, true)
    local on = debugging.drawing(DebugDrawings.drawer)
    debugging.setDrawing(DebugDrawings.drawer, before)
    self.results:set('switch', on and debugging.drawing(DebugDrawings.drawer) == before and 'pass' or 'fail', 'Switching a drawing', 'The drawing of the test turns on and back off.')
end

function DebugDrawings:update(dt)
    DebugDrawings.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    local drawings = debugging.drawings()
    self:status(string.format('Drawings on: %s   Statistics "%s"   %s', #drawings > 0 and table.concat(drawings, ', ') or 'none', debugging.statsMode(), self.results:summary()))
end

function DebugDrawings:fixedUpdate(step)
    self:simulate(self.world, step)
end

function DebugDrawings:draw(area)
    graphics2d.drawRect({-750, 380, 1500, 40}, '#FF4F5B6E')
    for _, body in ipairs(self.bodies) do
        graphics2d.draw(self.crate, body.x, body.y, {width = 56, height = 56, rotation = body.rotation})
    end
    graphics2d.draw(self.hero, self.bob.x, self.bob.y, {width = 72, height = 72, rotation = self.bob.rotation})
    for _, ball in ipairs(self.balls) do
        graphics2d.drawCircle(ball.x, ball.y, 28, '#FF64B5F6')
    end
    graphics2d.drawLine(520, -380, self.bob.x, self.bob.y, 4, '#FF7A8099')
    self.grab:draw()
end

return DebugDrawings
