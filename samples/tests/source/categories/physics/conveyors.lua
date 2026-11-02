-- Conveyor belts made with the tangent speed of their surface, carrying crates down a zigzag of belts that can speed up, slow down and turn around, into a bin at the end of the line. A crate that falls off the line onto the grass breaks.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Conveyors = haylen.class('Conveyors', PhysicsTest)

Conveyors.code = [[
local belt = world:createBody({type = 'static', x = -250, y = -250})
local surface = belt:addBox(800, 26, {friction = 0.9, tangentSpeed = 140})
surface.tangentSpeed = -surface.tangentSpeed  -- Turns the belt around.]]

-- Each belt with its middle, its length and the speed of its surface, and the post it hangs from.
Conveyors.belts = {{-250, -250, 800, 140, -1}, {250, -90, 800, -140, 1}, {-250, 70, 800, 140, -1}, {250, 230, 800, -160, 1}}
Conveyors.height = 26
Conveyors.chevronGap = 46
Conveyors.maxCrates = 30
Conveyors.ground = 360
Conveyors.post = 690
-- The bin under the end of the lowest belt, with its middle, its width and the top of its sides.
Conveyors.bin = {x = -240, width = 250, top = 290}
-- The rollers are steel with a hub whose bolts turn with the belt.
Conveyors.roller = Look.tint('steel', '#FFA9BCD3', '#FF7088A6', '#FFE9F2FB')
Conveyors.roller.hub, Conveyors.roller.hubShade = '#FFD6E0EE', '#FF5A6E8A'

function Conveyors:enter()
    self:frame{
        code = Conveyors.code,
        hint = 'Crates ride the belts from the top down. Drag them against the flow, and change the speed or turn the belts around. R or the X button clears the crates.',
        controls = {
            ui.label{text = 'Speed of the belts', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 1, min = 0, max = 3, step = 0.1, showValue = true, onChange = function(event) self:setSpeed(event.value, self.direction) end},
            ui.button{id = 'reverse', text = 'Turn the belts around', onClick = function() self:setSpeed(self.speed, -self.direction) end},
            ui.toggle{id = 'spawn', text = 'Drop crates', checked = true, onChange = function(event) self.spawning = event.checked end},
            ui.button{id = 'reset', text = 'Clear the crates', onClick = function() self:clear() end},
        },
        focus = 'speed',
    }
    self.random = m.random(29)
    self.speed, self.direction, self.spawning = 1, 1, true
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.breaks = Burst('level/crate-break', {count = 3, scale = 0.6, layer = 30})
    self.crates = {}
    self.belts = {}
    for _, spec in ipairs(Conveyors.belts) do
        local body = self.world:createBody({type = 'static', x = spec[1], y = spec[2]})
        local shape = body:addBox(spec[3], Conveyors.height, {friction = 0.9, tangentSpeed = spec[4]})
        self.belts[#self.belts + 1] = {body = body, shape = shape, width = spec[3], base = spec[4], post = spec[5], travel = 0}
    end
    local floor = self.world:createBody({type = 'static', x = 0, y = Conveyors.ground + 30})
    floor:addBox(8000, 60)
    local bin = Conveyors.bin
    for _, side in ipairs({-1, 1}) do
        floor:addBox(14, Conveyors.ground - bin.top, {offsetX = bin.x + side * (bin.width / 2 - 7), offsetY = (bin.top - Conveyors.ground) / 2 - 30})
    end
    for _, belt in ipairs(self.belts) do
        for index = 0, 2 do
            self:dropCrate(belt.body.x - 240 + index * 240, belt.body.y - 40)
        end
    end
    self.chevron = {0, 0, 0, 0, 0, 0}
    self.chevronStroke = {width = 1, color = '#CCFFC93C', join = 'round', cap = 'round', layer = 2}
    timer.every(0.8, function()
        if self.spawning then
            self:dropCrate()
        end
    end, {owner = self})
end

function Conveyors:setSpeed(speed, direction)
    self.speed, self.direction = speed, direction
    for _, belt in ipairs(self.belts) do
        belt.shape.tangentSpeed = belt.base * speed * direction
    end
end

function Conveyors:dropCrate(x, y)
    local start = self.direction > 0 and -600 or 600
    local crate = self.world:createBody({x = x or start + self.random:range(-40, 40), y = y or -400, rotation = self.random:range(-0.2, 0.2)})
    crate:addBox(46, 46, {friction = 0.8})
    Look.dress(crate, 'woodCrate')
    self.crates[#self.crates + 1] = crate
    if #self.crates > Conveyors.maxCrates then
        table.remove(self.crates, 1):destroy()
    end
end

-- A crate that drops into the bin goes once the front of the bin hides it, and a crate that lands on the grass breaks.
function Conveyors:collect()
    local bin = Conveyors.bin
    for index = #self.crates, 1, -1 do
        local crate = self.crates[index]
        local inBin = math.abs(crate.x - bin.x) < bin.width / 2
        if inBin and crate.y > bin.top + 26 then
            crate:destroy()
            table.remove(self.crates, index)
        elseif not inBin and crate.y > Conveyors.ground - 26 then
            self.breaks:fire(crate.x, crate.y)
            crate:destroy()
            table.remove(self.crates, index)
        end
    end
end

function Conveyors:clear()
    for _, crate in ipairs(self.crates) do
        crate:destroy()
    end
    self.crates = {}
end

function Conveyors:update(dt)
    Conveyors.super.update(self, dt)
    if input.pressed('reset') then
        self:clear()
    end
    self.grab:update(self.pointer, self.camera)
    self:collect()
    self.breaks:update(dt)
    for _, belt in ipairs(self.belts) do
        belt.travel = belt.travel + belt.shape.tangentSpeed * dt
    end
    local top = self.belts[1].shape.tangentSpeed
    self:status(string.format('The belts run at %.1f times their speed, the top one to the %s, with %d crates on the way.', self.speed, top >= 0 and 'right' or 'left', #self.crates))
end

function Conveyors:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- A belt is a band of rubber around a roller at each end that turns with its surface, with chevrons that run along it the way it moves.
function Conveyors:drawBelt(belt)
    local unit = Look.pixel
    local x, y, width, height = belt.body.x, belt.body.y, belt.width, Conveyors.height
    local left = x - width / 2
    graphics2d.drawShape({left, y - height / 2, width, height}, {radius = height / 2, color = '#FF3A3D4E', borderWidth = 3.5 * unit, borderColor = Look.ink, layer = 1})
    local spin = belt.travel / (height / 2)
    for _, rx in ipairs({left + height / 2, left + width - height / 2}) do
        Look.disc(rx, y, height / 2 - 4 * unit, Conveyors.roller, 1, spin)
    end
    local speed = belt.shape.tangentSpeed
    if speed == 0 then
        return
    end
    local direction = speed > 0 and 1 or -1
    local chevron, stroke = self.chevron, self.chevronStroke
    stroke.width = 4 * unit
    graphics2d.pushClip({left + height, y - height / 2, width - height * 2, height})
    local shift = belt.travel % Conveyors.chevronGap
    for cx = left + shift - Conveyors.chevronGap, left + width, Conveyors.chevronGap do
        chevron[1], chevron[2] = cx - 5 * direction, y - 7
        chevron[3], chevron[4] = cx + 4 * direction, y
        chevron[5], chevron[6] = cx - 5 * direction, y + 7
        graphics2d.drawStroke(chevron, stroke)
    end
    graphics2d.popClip()
end

-- The belts hang from two posts at the ends of the hall, each from the end the crates do not fall from, on an arm with a brace that runs from the post up to the underside of the belt.
function Conveyors:drawFrame()
    for _, belt in ipairs(self.belts) do
        local endX = belt.body.x + belt.post * belt.width / 2
        local postX = belt.post * Conveyors.post
        local fromY, toX, toY = belt.body.y + 80, endX - belt.post * 60, belt.body.y + Conveyors.height / 2
        local dx, dy = toX - postX, toY - fromY
        Look.block((postX + toX) / 2, (fromY + toY) / 2, math.sqrt(dx * dx + dy * dy), 10, 'iron', 0, math.atan(dy, dx))
        Look.block((endX + postX) / 2, belt.body.y, math.abs(postX - endX) + 20, 14, 'iron', 0)
    end
    for _, side in ipairs({-1, 1}) do
        Look.block(side * Conveyors.post, (Conveyors.ground - 330) / 2, 30, Conveyors.ground + 330, 'steel', 0)
    end
end

-- The bin is a wooden box whose back shows behind the crates that drop in and whose front hides them.
function Conveyors:drawBin()
    local bin = Conveyors.bin
    local height = Conveyors.ground - bin.top
    local y = bin.top + height / 2
    Look.block(bin.x, y - 6, bin.width - 20, height - 12, 'darkWood', 2)
    Look.block(bin.x, y, bin.width, height, 'wood', 4)
end

function Conveyors:draw(area)
    Scenery.sky(self.camera, Conveyors.ground)
    Scenery.ground(Conveyors.ground)
    self:drawFrame()
    self:drawBin()
    for _, belt in ipairs(self.belts) do
        self:drawBelt(belt)
    end
    Look.drawAll(self.crates, {layer = 3})
    self.breaks:draw()
    self.grab:draw()
end

return Conveyors
