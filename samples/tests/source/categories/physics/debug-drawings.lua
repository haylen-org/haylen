-- Debug drawings: crates and a barrel drawn as sprites fall on the ground under a swinging barrel and a pair of coins on a spring, while the drawings of "haylen.debug" switch on and off. The drawing "physics" outlines every body, shape, joint and contact of the world, "bounds" outlines every sprite with the path of its texture, and a drawer of the test marks the center of every body, all over the canvas they belong to. The full statistics show the debug overlay with the scenes of the stack and a checkbox for every drawing.
local assets = require('haylen.assets')
local debugging = require('haylen.debug')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Results = require('harness.results')
local Scenery = require('categories.physics.scenery')

local DebugDrawings = haylen.class('DebugDrawings', PhysicsTest)

DebugDrawings.drawer = 'centers'
DebugDrawings.crates = 14
DebugDrawings.crate = 60
DebugDrawings.ground = 360
DebugDrawings.pivot = {520, -300}
DebugDrawings.modes = {{id = 'off', text = 'Off'}, {id = 'compact', text = 'Compact'}, {id = 'full', text = 'Full'}}
DebugDrawings.names = {physics = 'the physics shapes', bounds = 'the sprite bounds', centers = 'the centers'}
DebugDrawings.code = [[
debugging.setDrawing('physics', true)  -- Or press F4, or list it in "debug.drawings" of "app.json".
debugging.addDrawer('centers', function(kind) graphics2d.drawCircle(x, y, 6, '#FFFF4080') end, {owner = self})
debugging.setStatsMode('full')  -- The overlay lists the scenes and the drawings.]]

function DebugDrawings:enter()
    self.crate = assets.texture('physics/crate.png', {filter = 'linear'})
    self.barrel = assets.texture('physics/barrel.png', {filter = 'linear'})
    self.coin = assets.texture('physics/coin.png', {filter = 'linear'})
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
        code = DebugDrawings.code,
        hint = 'Drag any body. F4 switches every drawing on desktops, and F3 cycles the statistics.',
        controls = {
            ui.checkbox{id = 'physics', text = 'Physics shapes', checked = debugging.drawing('physics'), onChange = function(event) debugging.setDrawing('physics', event.checked) end},
            ui.checkbox{id = 'bounds', text = 'Sprite bounds', checked = debugging.drawing('bounds'), onChange = function(event) debugging.setDrawing('bounds', event.checked) end},
            ui.checkbox{id = 'centers', text = 'Centers, drawn by the test', checked = false, onChange = function(event) debugging.setDrawing(DebugDrawings.drawer, event.checked) end},
            ui.formField{label = 'Statistics', ui.segmentedControl{id = 'stats', items = DebugDrawings.modes, selected = self.mode, onChange = function(event) debugging.setStatsMode(event.value) end}},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'physics',
    }
    self.rope = {0, 0, 0, 0}
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

    local ground = self.world:createBody({type = 'static', x = 0, y = DebugDrawings.ground + 30})
    ground:addBox(8000, 60)
    for _ = 1, DebugDrawings.crates do
        local body = self.world:createBody({x = random:range(-560, 260), y = random:range(-380, 100), rotation = random:range(0, math.pi)})
        body:addBox(DebugDrawings.crate, DebugDrawings.crate, {density = 1, friction = 0.6})
        self.bodies[#self.bodies + 1] = body
    end

    local pivot = DebugDrawings.pivot
    local anchor = self.world:createBody({type = 'static', x = pivot[1], y = pivot[2]})
    self.bob = self.world:createBody({x = pivot[1] + 180, y = pivot[2] + 120})
    self.bob:addBox(52, 62, {density = 2})
    self.world:createJoint('revolute', anchor, self.bob, {ax = pivot[1], ay = pivot[2]})

    local left = self.world:createBody({x = 460, y = 120})
    left:addCircle(30)
    local right = self.world:createBody({x = 640, y = 120})
    right:addCircle(30)
    self.world:createJoint('distance', left, right, {ax = 460, ay = 120, bx = 640, by = 120, enableSpring = true, hertz = 2, dampingRatio = 0.2})
    self.coins = {left, right}
    self.bodies[#self.bodies + 1] = self.bob
    self.bodies[#self.bodies + 1] = left
    self.bodies[#self.bodies + 1] = right
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
    local shown = {}
    for _, name in ipairs(debugging.drawings()) do
        shown[#shown + 1] = DebugDrawings.names[name] or name
    end
    local drawings = #shown > 0 and 'The drawings show ' .. table.concat(shown, ', ') or 'No drawing is on'
    self:status(string.format('%s, and the statistics are %s.', drawings, debugging.statsMode() == 'off' and 'hidden' or debugging.statsMode()))
end

function DebugDrawings:fixedUpdate(step)
    self:simulate(self.world, step)
end

function DebugDrawings:draw(area)
    Scenery.sky(self.camera, DebugDrawings.ground)
    Scenery.ground(DebugDrawings.ground)
    local size = DebugDrawings.crate * 160 / 152
    for index = 1, DebugDrawings.crates do
        local body = self.bodies[index]
        graphics2d.draw(self.crate, body.x, body.y, {width = size, height = size, rotation = body.rotation, layer = 1})
    end
    local pivot = DebugDrawings.pivot
    local unit = Look.pixel
    local bob = self.bob
    self.rope[1], self.rope[2], self.rope[3], self.rope[4] = pivot[1], pivot[2], bob.x, bob.y
    Look.rope(self.rope, 10, 1)
    Look.block(pivot[1], pivot[2] - 16, 120, 20, 'darkWood', 2)
    Look.pin(pivot[1], pivot[2], 8 * unit, 3)
    graphics2d.draw(self.barrel, bob.x, bob.y, {width = 52 * 164 / 156, height = 62 * 192 / 184, rotation = bob.rotation, layer = 2})
    local a, b = self.coins[1], self.coins[2]
    Look.spring(a.x, a.y, b.x, b.y, 24, 8, '#FFA9BCD3', 1)
    for _, coin in ipairs(self.coins) do
        graphics2d.draw(self.coin, coin.x, coin.y, {width = 60 * 96 / 88, height = 60 * 96 / 88, rotation = coin.rotation, layer = 2})
    end
    self.grab:draw()
end

return DebugDrawings
