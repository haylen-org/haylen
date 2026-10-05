-- The pond: a school of fish over a pebbled floor under moving light, with lily pads above. The panel adds and removes fish ten thousand at a time and switches how many fish move each frame and how their sprites reach the renderer, and the pointer, a finger or a stick leads the school.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Hud = require('hud')
local Swarm = require('swarm')
local Tour = require('tour')

local Pond = haylen.class('Pond', scene.Scene)

Pond.start = 20000
Pond.step = 10000
Pond.leadSpeed = 900
Pond.manualTime = 2.5
Pond.returnRate = 1.2
-- Lily pads as fractions of the play area, with their size, their turn and whether a blossom sits on them.
Pond.pads = {
    {x = 0.08, y = 0.12, size = 210, turn = 0.4, blossom = true},
    {x = 0.22, y = 0.06, size = 130, turn = 2.1},
    {x = 0.9, y = 0.2, size = 170, turn = 4.0},
    {x = 0.78, y = 0.9, size = 240, turn = 1.2, blossom = true},
    {x = 0.64, y = 0.98, size = 140, turn = 3.1},
    {x = 0.06, y = 0.84, size = 180, turn = 5.2},
}
-- The two ways to change the load besides the count: how many fish move each frame, and how their sprites reach the renderer.
Pond.simulations = {{id = 'full', text = 'Every frame'}, {id = 'staggered', text = 'Staggered'}, {id = 'frozen', text = 'Frozen'}}
Pond.handOvers = {{id = 'buffer', text = 'Buffer'}, {id = 'batch', text = 'Batch'}, {id = 'tables', text = 'Tables'}}
Pond.names = {full = 'every fish every frame', staggered = 'half the fish every frame', frozen = 'frozen', buffer = 'a float buffer', batch = 'a sprite batch', tables = 'one table each'}
Pond.help = {
    full = 'Every fish moves every frame, which costs the Lua work of the whole school.',
    staggered = 'Half the school moves each frame by the time of two frames, which halves the Lua work and keeps the motion smooth.',
    frozen = 'Nothing moves and nothing is handed over, which leaves only what drawing costs.',
    buffer = 'One copy hands the Lua arrays to a float buffer, and one "drawBatch" call draws the school from it.',
    batch = 'The same copy moves a sprite batch with "writeFields", and the batch keeps the other fields of its sprites in C++.',
    tables = 'Every fish is a Lua table that the renderer reads key by key, the slow way.',
}
Pond.actions = {actions = {
    {name = 'more', type = 'button', bindings = {'key:equal', 'key:keypadAdd', 'axis:rightTrigger+', 'button:rightShoulder'}},
    {name = 'fewer', type = 'button', bindings = {'key:minus', 'key:keypadSubtract', 'axis:leftTrigger+', 'button:leftShoulder'}},
    {name = 'handOver', type = 'button', bindings = {'key:m', 'button:north'}},
    {name = 'simulation', type = 'button', bindings = {'key:n', 'button:rightStick'}},
    {name = 'lead', type = 'vector', up = {'key:up', 'key:w'}, down = {'key:down', 'key:s'}, left = {'key:left', 'key:a'}, right = {'key:right', 'key:d'}, bindings = {'stick:left', 'stick:right'}},
    {name = 'uiFocus', type = 'button', bindings = {'button:back', 'key:pause'}},
}}

local function raster(path)
    local texture, failure = assets.vectorImage(path):rasterize(2):await()
    if not texture then
        error(failure, 0)
    end
    return texture
end

function Pond:load(context)
    context:progress(0, 'Drawing the fish')
    self.fishTexture = raster('sprites/fish.svg')
    context:progress(0.5, 'Drawing the pond')
    self.floorTexture = raster('pond/pond_floor.svg')
    self.lightTexture = raster('pond/caustics.svg')
    self.pad = assets.vectorImage('pond/lily_pad.svg')
    self.blossom = assets.vectorImage('pond/lily_blossom.svg')
end

function Pond:enter()
    input.loadActions(Pond.actions)
    self.camera = graphics2d.newCamera()
    self.camera.anchor = 'topLeft'
    self.floor = graphics2d.newParallax(self.floorTexture, {size = {256, 256}, repeatX = true, repeatY = true})
    self.lights = {
        graphics2d.newParallax(self.lightTexture, {size = {320, 320}, repeatX = true, repeatY = true, autoscroll = {16, 9}, color = '#24FFFFFF'}),
        graphics2d.newParallax(self.lightTexture, {size = {460, 460}, position = {90, 40}, repeatX = true, repeatY = true, autoscroll = {-12, 14}, color = '#1AFFFFFF'}),
    }
    self.swarm = Swarm.new(self.fishTexture, m.random(7))
    self.time = 0
    self.manual = 0
    self.hud = Hud.new(self, {
        title = 'Many Sprites',
        caption = 'A school of animated fish, every one a sprite with its own position, heading and frame.',
        rows = {
            {id = 'fish', label = 'Fish'},
            {id = 'sprites', label = 'Sprites drawn'},
            {id = 'simulationTime', label = 'Lua simulation'},
            {id = 'handOverTime', label = 'Hand-over'},
        },
        controls = {
            ui.row{gap = 12,
                ui.button{id = 'add', text = 'Add ' .. Hud.count(Pond.step), variant = 'primary', grow = 1, onClick = function() self:addFish() end},
                ui.button{id = 'remove', text = 'Remove ' .. Hud.count(Pond.step), grow = 1, onClick = function() self:removeFish() end},
            },
            ui.label{text = 'Simulation', font = 'caption', color = 'textMuted'},
            ui.segmentedControl{id = 'simulation', items = Pond.simulations, selected = 'full', onChange = function(event) self:setSimulation(event.value) end},
            ui.label{id = 'simulationHelp', text = Pond.help.full, font = 'caption'},
            ui.label{text = 'Hand-over to the renderer', font = 'caption', color = 'textMuted'},
            ui.segmentedControl{id = 'handOverMode', items = Pond.handOvers, selected = 'buffer', onChange = function(event) self:setHandOver(event.value) end},
            ui.label{id = 'handOverHelp', text = Pond.help.buffer, font = 'caption'},
        },
        hint = 'Plus and minus or the triggers add and remove fish, N or a click of the right stick changes the simulation, and M or the north button changes the hand-over. Click or touch the pond, or steer with a stick or the arrows while the pond has the focus, to lead the school. Tab, the View button or Play/Pause moves the focus between the pond and the panel.',
        focus = 'add',
    })
    if haylen.platform == 'headless' then
        Tour.start(self, self:tourSteps())
    end
end

-- The steps of the automatic run: every mode, more fish, fewer fish and none at all.
function Pond:tourSteps()
    local function step(change)
        return function()
            change()
            local swarm = self.swarm
            return string.format('%s fish, %s, handed over through %s', Hud.count(swarm.count), Pond.names[swarm.simulation], Pond.names[swarm.handOverMode])
        end
    end
    return {
        step(function() end),
        step(function() self:addFish() end),
        step(function() self:setHandOver('batch') end),
        step(function() self:setHandOver('tables') end),
        step(function() self:nextHandOver() end),
        step(function() self:setSimulation('staggered') end),
        step(function() self:nextSimulation() end),
        step(function() self:nextSimulation() end),
        step(function() self.swarm:remove(self.swarm.count) end),
        step(function() self:addFish() end),
    }
end

function Pond:reach()
    return math.min(self.stage.width, self.stage.height) * 0.42
end

function Pond:addFish()
    self.swarm:add(Pond.step, self.baitX, self.baitY, self:reach())
end

function Pond:removeFish()
    self.swarm:remove(Pond.step)
end

function Pond:setSimulation(mode)
    self.swarm:setSimulation(mode)
    self.hud:set('simulation', 'selected', mode)
    self.hud:show('simulationHelp', Pond.help[mode])
end

function Pond:setHandOver(mode)
    self.swarm:setHandOver(mode)
    self.hud:set('handOverMode', 'selected', mode)
    self.hud:show('handOverHelp', Pond.help[mode])
end

local function following(items, id)
    for index, item in ipairs(items) do
        if item.id == id then
            return items[index % #items + 1].id
        end
    end
end

function Pond:nextSimulation()
    self:setSimulation(following(Pond.simulations, self.swarm.simulation))
end

function Pond:nextHandOver()
    self:setHandOver(following(Pond.handOvers, self.swarm.handOverMode))
end

-- The point the school circles when nobody leads it, a slow loop through the play area.
function Pond:wanderPoint()
    local stage, time = self.stage, self.time
    return stage.x + stage.width * (0.5 + math.sin(time * 0.31) * 0.27 + math.sin(time * 0.83) * 0.06),
        stage.y + stage.height * (0.5 + math.sin(time * 0.47 + 1) * 0.25 + math.cos(time * 1.1) * 0.05)
end

-- Returns the point a mouse button or a finger holds down on the play area, if any.
function Pond:heldPoint()
    for _, touch in ipairs(input.touches()) do
        if touch.phase ~= 'ended' and touch.phase ~= 'cancelled' and self.stage:contains({touch.x, touch.y}) then
            return touch.x, touch.y
        end
    end
    if input.mouseDown('left') and not ui.usingPointer() then
        local x, y = input.mousePosition()
        if self.stage:contains({x, y}) then
            return x, y
        end
    end
end

-- The pointer, a finger or a stick leads the bait, and a little after the last input it drifts back to its loop.
function Pond:lead(dt)
    local heldX, heldY = self:heldPoint()
    local dx, dy = input.vector('lead')
    if heldX then
        self.baitX, self.baitY = heldX, heldY
        self.manual = Pond.manualTime
    elseif dx ~= 0 or dy ~= 0 then
        self.baitX = m.clamp(self.baitX + dx * Pond.leadSpeed * dt, self.stage.x, self.stage.x + self.stage.width)
        self.baitY = m.clamp(self.baitY + dy * Pond.leadSpeed * dt, self.stage.y, self.stage.y + self.stage.height)
        self.manual = Pond.manualTime
    elseif self.manual > 0 then
        self.manual = self.manual - dt
    else
        local x, y = self:wanderPoint()
        local blend = m.dampFactor(Pond.returnRate, dt)
        self.baitX, self.baitY = m.lerp(self.baitX, x, blend), m.lerp(self.baitY, y, blend)
    end
end

function Pond:update(dt)
    local stage = self.hud:stage()
    if not stage then
        return
    end
    self.stage = stage
    if not self.baitX then
        self.baitX, self.baitY = self:wanderPoint()
        self.swarm:add(Pond.start, self.baitX, self.baitY, self:reach())
    end

    if input.pressed('more') then
        self:addFish()
    end
    if input.pressed('fewer') then
        self:removeFish()
    end
    if input.pressed('simulation') then
        self:nextSimulation()
    end
    if input.pressed('handOver') then
        self:nextHandOver()
    end
    self.time = self.time + dt
    self:lead(dt)
    self.swarm:update(dt, self.baitX, self.baitY)
    for _, light in ipairs(self.lights) do
        light:update(dt)
    end

    if self.hud:update(dt) then
        self.hud:show('fish', Hud.count(self.swarm.count))
        self.hud:show('sprites', Hud.count(self.hud.stats.rendering.sprites))
        self.hud:show('simulationTime', Hud.milliseconds(Hud.scope('swarm')))
        self.hud:show('handOverTime', Hud.milliseconds(Hud.scope('handOver')))
    end
end

function Pond:render()
    if not self.stage then
        return
    end
    local visible = viewport.visibleRect()
    self.camera.x, self.camera.y = visible.x, visible.y
    graphics2d.beginWorld(self.camera)
    self.floor:draw(self.camera, {layer = 0})
    for _, light in ipairs(self.lights) do
        light:draw(self.camera, {layer = 1, blend = 'additive'})
    end
    self.swarm:draw()

    if self.manual > 0 then
        local pulse = (self.time * 1.5) % 1
        graphics2d.drawRing(self.baitX, self.baitY, 18 + pulse * 46, 3, m.color(1, 1, 1, 0.5 * (1 - pulse)), {layer = 3})
    end
    local stage = self.stage
    for index, pad in ipairs(Pond.pads) do
        local x, y = stage.x + stage.width * pad.x, stage.y + stage.height * pad.y
        local sway = math.sin(self.time * 0.4 + index) * 0.06
        graphics2d.drawVector(self.pad, x, y, {width = pad.size, height = pad.size, rotation = pad.turn + sway, layer = 4})
        if pad.blossom then
            graphics2d.drawVector(self.blossom, x - pad.size * 0.1, y - pad.size * 0.08, {width = pad.size * 0.45, height = pad.size * 0.45, rotation = sway * 2, layer = 5})
        end
    end
end

return Pond
