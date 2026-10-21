-- Playground: a room to try every value of a body and of the world, with a card beside the chosen body that reads its mass, material, damping, gravity scale and speed. A click or a tap drops a body with the values of the panel, grabs it, pushes it with an impulse dragged from it or fires a shot at it, and the panel changes the chosen body as it moves.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Playground = haylen.class('Playground', PhysicsTest)

Playground.code = [[
world.gravity = {0, 980}  -- Writing it wakes every body.
local body = world:createBody({x = x, y = y, linearDamping = 0.2, angularDamping = 0.5, gravityScale = 1})
body:addBox(60, 60, {density = 1, friction = 0.6, restitution = 0.3})
for _, shape in ipairs(body:shapes()) do shape.restitution = 0.9 end  -- The material changes while it moves.
body:applyImpulse(dragX * strength, dragY * strength)  print(body.mass, body.velocity:length())]]

Playground.tools = {{id = 'drop', text = 'Drop'}, {id = 'grab', text = 'Grab'}, {id = 'push', text = 'Push'}, {id = 'shoot', text = 'Shoot'}}
Playground.kinds = {{id = 'box', text = 'Box'}, {id = 'ball', text = 'Ball'}, {id = 'triangle', text = 'Triangle'}}
Playground.ground = 400
Playground.ramp = {{200, 400}, {780, 160}, {780, 400}}
Playground.materials = {box = 'blue', ball = 'yellow', triangle = 'green'}
-- The cannon in the corner turns its barrel around the trunnion toward the pointer, and its picture reaches the muzzle the length of the barrel ahead.
Playground.cannon = {-720, 340}
Playground.barrelLength = 90
Playground.picture = {width = 320, height = 166, pivot = 0.425, muzzle = 180}
Playground.cardLines = 7

function Playground:enter()
    self.tool = 'drop'
    self.kind = 'box'
    self.barrel = assets.texture('physics/cannon.png', {filter = 'linear'})
    self.aim = -0.3
    self.rampShape = Look.newShape(Playground.ramp, 'stone')
    self.cardShape = {radius = 10, color = '#EE2F3352', borderWidth = 2, borderColor = '#FF15172A', layer = 30}
    self.cardRect = {0, 0, 0, 0}
    self.cardText = {size = 1, color = '#FFFFFFFF', anchor = {0, 0}, layer = 31}
    self.values = {density = 1, friction = 0.6, restitution = 0.3, linearDamping = 0, angularDamping = 0.1, gravityScale = 1, gravityX = 0, gravityY = 980, push = 6, shot = 1400}
    self:build()

    local function slider(id, label, min, max, step, decimals)
        return ui.formField{label = label, ui.slider{id = id, min = min, max = max, step = step, value = self.values[id], showValue = true, decimals = decimals, onChange = function(event)
            self:change(id, event.value)
        end}}
    end
    self:frame{
        code = Playground.code,
        hint = 'Pick a tool and click or tap the room. The sliders set the next body and change the chosen one, whose card reads its values. R or the X button empties the room.',
        controls = {
            ui.segmentedControl{id = 'tool', items = Playground.tools, selected = 'drop', onChange = function(event) self:pickTool(event.value) end},
            ui.segmentedControl{id = 'kind', items = Playground.kinds, selected = 'box', onChange = function(event) self.kind = event.value end},
            ui.sectionTitle{text = 'The body'},
            slider('density', 'Density', 0.1, 20, 0.1, 1),
            slider('friction', 'Friction', 0, 1.5, 0.05, 2),
            slider('restitution', 'Restitution', 0, 1, 0.05, 2),
            slider('linearDamping', 'Linear damping', 0, 5, 0.1, 1),
            slider('angularDamping', 'Angular damping', 0, 5, 0.1, 1),
            slider('gravityScale', 'Gravity scale', -1, 3, 0.1, 1),
            ui.sectionTitle{text = 'The world and the tools'},
            slider('gravityX', 'Gravity across', -1000, 1000, 20, 0),
            slider('gravityY', 'Gravity down', -1500, 2000, 20, 0),
            slider('push', 'Strength of a push', 1, 20, 0.5, 1),
            slider('shot', 'Speed of a shot', 300, 3000, 50, 0),
            ui.button{id = 'empty', text = 'Empty the room', onClick = function() self:build() end},
        },
    }
    self.camera.position = {0, 30}
end

-- Builds the room: a floor, walls and a ceiling, so bodies that fall up stay inside, a ramp and a row of pegs, and two bodies to start with.
function Playground:build()
    self.world = physics2d.newWorld({gravity = {self.values.gravityX, self.values.gravityY}})
    self.grab = Grab(self.world)
    self.bodies, self.shots = {}, {}
    local room = self.world:createBody({type = 'static'})
    for _, wall in ipairs({{0, Playground.ground + 20, 1600, 40}, {0, -440, 1600, 40}, {-800, 0, 40, 900}, {800, 0, 40, 900}}) do
        room:addBox(wall[3], wall[4], {offsetX = wall[1], offsetY = wall[2]})
    end
    room:addPolygon(Playground.ramp)
    self.pegs = {}
    for index = 0, 5 do
        local x, y = -300 + index * 90, -60 + (index % 2) * 40
        room:addCircle(12, {offsetX = x, offsetY = y})
        self.pegs[#self.pegs + 1] = {x, y}
    end
    self.chosen = nil
    self:drop(-200, -300)
    self:drop(60, -300)
end

function Playground:pickTool(tool)
    self.tool = tool
    self.grab:release()
    self.pushing = nil
end

-- Drops a body of the chosen kind with the values of the panel, and chooses it.
function Playground:drop(x, y)
    local values = self.values
    local body = self.world:createBody({x = x, y = y, linearDamping = values.linearDamping, angularDamping = values.angularDamping, gravityScale = values.gravityScale})
    local material = {density = values.density, friction = values.friction, restitution = values.restitution}
    if self.kind == 'ball' then
        body:addCircle(32, material)
    elseif self.kind == 'triangle' then
        body:addPolygon({{0, -40}, {40, 30}, {-40, 30}}, material)
    else
        body:addBox(64, 64, material)
    end
    Look.dress(body, Playground.materials[self.kind], {spin = self.kind == 'ball'})
    self.bodies[#self.bodies + 1] = body
    self.chosen = body
end

-- Returns the dynamic body under the pointer, or nothing.
function Playground:bodyAt(x, y)
    for _, shape in ipairs(self.world:queryPoint(x, y)) do
        if shape.body.type == 'dynamic' then
            return shape.body
        end
    end
end

-- Applies a value of the panel: the world gravity to the world, the tools to the next use, and the body values to the next body and to the chosen one.
function Playground:change(id, value)
    self.values[id] = value
    if id == 'gravityX' or id == 'gravityY' then
        self.world.gravity = {self.values.gravityX, self.values.gravityY}
        return
    end
    local body = self.chosen
    if not body or not body.valid then
        return
    end
    if id == 'density' or id == 'friction' or id == 'restitution' then
        for _, shape in ipairs(body:shapes()) do
            shape[id] = value
        end
    elseif id == 'linearDamping' or id == 'angularDamping' or id == 'gravityScale' then
        body[id] = value
    end
    body.awake = true
end

-- Returns the trunnion of the cannon and the direction its barrel points.
function Playground:muzzle()
    local cannon = Playground.cannon
    return cannon[1], cannon[2], math.cos(self.aim), math.sin(self.aim)
end

-- Fires a fast iron ball from the muzzle of the cannon toward the pointer, as a bullet that never passes through a thin body.
function Playground:shoot()
    local x, y, dx, dy = self:muzzle()
    local speed = self.values.shot
    local length = Playground.barrelLength
    local ball = self.world:createBody({x = x + dx * length, y = y + dy * length, bullet = true, vx = dx * speed, vy = dy * speed})
    ball:addCircle(12, {density = 4, restitution = 0.2})
    Look.dress(ball, 'iron')
    self.shots[#self.shots + 1] = ball
    -- The oldest shots leave, so a long stream of them never fills the room.
    if #self.shots > 30 then
        table.remove(self.shots, 1):destroy()
    end
end

function Playground:update(dt)
    Playground.super.update(self, dt)
    if not self.stage then
        return
    end
    if input.pressed('reset') then
        self:build()
    end

    local pointer = self.pointer
    local x, y = pointer.worldX, pointer.worldY
    local cannon = Playground.cannon
    self.aim = math.max(-1.45, math.min(0.3, math.atan(y - cannon[2], math.max(x - cannon[1], 1))))
    if self.tool == 'grab' then
        if pointer.pressed then
            self.chosen = self:bodyAt(x, y) or self.chosen
        end
        self.grab:update(pointer, self.camera)
    elseif pointer.pressed and not ui.usingPointer() then
        if self.tool == 'drop' then
            self:drop(x, y)
        elseif self.tool == 'shoot' then
            self:shoot()
        elseif self.tool == 'push' then
            local body = self:bodyAt(x, y)
            if body then
                self.chosen, self.pushing = body, {x, y}
            end
        end
    end
    -- A push pulls back like a slingshot: the impulse points from the pointer to where the press started.
    if self.pushing and pointer.released then
        local body = self.chosen
        if body and body.valid then
            local strength = self.values.push * body.mass
            body:applyImpulse((self.pushing[1] - x) * strength, (self.pushing[2] - y) * strength)
        end
        self.pushing = nil
    end

    self:simulate(self.world, dt)
    self:status(string.format('%d bodies and %d shots are in the room, under a gravity of %d across and %d down.', #self.bodies, #self.shots, self.values.gravityX, self.values.gravityY))
end

-- Draws the card of the chosen body beside it, with the values it holds now, inside what the camera shows.
function Playground:drawCard(body)
    local unit = Look.pixel
    local bounds = graphics2d.canvasBounds()
    local shape = body:shapes()[1]
    local lines = string.format('Mass %.2f kg\nDensity %.1f, friction %.2f\nRestitution %.2f\nDamping %.1f, spin damping %.1f\nGravity scale %.1f\nSpeed %d units a second\nSpin %.1f radians a second', body.mass, shape.density, shape.friction, shape.restitution, body.linearDamping, body.angularDamping, body.gravityScale, PhysicsTest.round(body.velocity:length()), body.angularVelocity)
    local width, height = 330 * unit, (Playground.cardLines * 27 + 24) * unit
    local x = math.min(bounds.x + bounds.width - width - 16 * unit, body.x + 70)
    local y = math.max(bounds.y + 16 * unit, math.min(bounds.y + bounds.height - height - 16 * unit, body.y - height / 2))
    local rect = self.cardRect
    rect[1], rect[2], rect[3], rect[4] = x, y, width, height
    self.cardShape.borderWidth = 2 * unit
    graphics2d.drawShape(rect, self.cardShape)
    self.cardText.size = 20 * unit
    graphics2d.drawText(nil, lines, x + 14 * unit, y + 12 * unit, self.cardText)
    Look.dashed(body.x, body.y, x, y + height / 2, '#CCFFFFFF', 29)
end

function Playground:draw(area)
    local bounds = graphics2d.canvasBounds()
    Scenery.sky(self.camera, Playground.ground)
    Scenery.ground(Playground.ground)
    for _, x in ipairs({-800, 800}) do
        Look.block(x, (bounds.y + Playground.ground) / 2, 40, Playground.ground - bounds.y + 20, 'stone', -12)
    end
    Look.drawShape(self.rampShape, 0)
    for _, peg in ipairs(self.pegs) do
        Look.disc(peg[1], peg[2], 12, 'steel', 0)
    end
    Look.drawAll(self.bodies, {layer = 1})
    Look.drawAll(self.shots, {layer = 1})

    -- The cannon is a barrel that turns toward the pointer on a wooden wheel.
    local x, y, dx, dy = self:muzzle()
    local picture = Playground.picture
    local scale = Playground.barrelLength / picture.muzzle
    graphics2d.draw(self.barrel, x, y, {width = picture.width * scale, height = picture.height * scale, pivotX = picture.pivot, pivotY = 0.5, rotation = self.aim, layer = 2})
    Look.disc(x, y + 30, 30, 'wheel', 3)
    if self.tool == 'shoot' then
        local length = Playground.barrelLength
        Look.dashed(x + dx * length, y + dy * length, self.pointer.worldX, self.pointer.worldY, '#B3F0473E', 2)
    end
    if self.pushing then
        Look.line(self.pushing[1], self.pushing[2], self.pointer.worldX, self.pointer.worldY, 6, '#FFFFC93C', 20)
    end
    if self.chosen and self.chosen.valid then
        graphics2d.drawRing(self.chosen.x, self.chosen.y, 54, 3 * Look.pixel, '#FFFFFFFF', {layer = 20})
        self:drawCard(self.chosen)
    end
    self.grab:draw()
end

return Playground
