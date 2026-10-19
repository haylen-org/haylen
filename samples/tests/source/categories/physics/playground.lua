-- Playground: a room to try every value of a body and of the world, with a card beside the chosen body that reads its mass, material, damping, gravity scale and speed. A click or a tap drops a body with the values of the panel, grabs it, pushes it with an impulse dragged from it or fires a shot at it, and the panel changes the chosen body as it moves.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Playground = haylen.class('Playground', PhysicsTest)

Playground.code = [[
world.gravity = {0, 980}  -- Writing it wakes every body.
local body = world:createBody({x = x, y = y, linearDamping = 0.2, angularDamping = 0.5, gravityScale = 1})
body:addBox(60, 60, {density = 1, friction = 0.6, restitution = 0.3})
for _, shape in ipairs(body:shapes()) do shape.restitution = 0.9 end  -- The material changes while it moves.
body:applyImpulse(dragX * strength, dragY * strength)  print(body.mass, body.velocity:length())]]

Playground.tools = {{id = 'drop', text = 'Drop'}, {id = 'grab', text = 'Grab'}, {id = 'push', text = 'Push'}, {id = 'shoot', text = 'Shoot'}}
Playground.kinds = {{id = 'box', text = 'Box'}, {id = 'ball', text = 'Ball'}, {id = 'triangle', text = 'Triangle'}}
Playground.card = {radius = 10, color = '#E61B2430', borderWidth = 2, borderColor = '#FF2A3646', layer = 30}
Playground.cardText = {size = 22, color = '#FFE8EAF2', anchor = {0, 0}, layer = 31}
Playground.cannon = {-740, 300}

function Playground:enter()
    self.tool = 'drop'
    self.kind = 'box'
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
end

-- Builds the room: a floor, walls and a ceiling, so bodies that fall up stay inside, a ramp and a row of pegs, and two bodies to start with.
function Playground:build()
    self.world = physics2d.newWorld({gravity = {self.values.gravityX, self.values.gravityY}})
    self.grab = Grab(self.world)
    self.statics, self.bodies, self.shots = {}, {}, {}
    for _, wall in ipairs({{0, 420, 1600, 40}, {0, -440, 1600, 40}, {-800, 0, 40, 900}, {800, 0, 40, 900}}) do
        local body = self.world:createBody({type = 'static', x = wall[1], y = wall[2]})
        parts.box(body, wall[3], wall[4])
        self.statics[#self.statics + 1] = body
    end
    local ramp = self.world:createBody({type = 'static'})
    parts.polygon(ramp, {{200, 400}, {780, 160}, {780, 400}})
    self.statics[#self.statics + 1] = ramp
    for index = 0, 5 do
        local peg = self.world:createBody({type = 'static', x = -300 + index * 90, y = -60 + (index % 2) * 40})
        parts.circle(peg, 12)
        self.statics[#self.statics + 1] = peg
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
        parts.circle(body, 32, material)
    elseif self.kind == 'triangle' then
        parts.polygon(body, {{0, -40}, {40, 30}, {-40, 30}}, material)
    else
        parts.box(body, 64, 64, material)
    end
    parts.paint(body, ({box = '#FF3C9BE0', ball = '#FFE8A93A', triangle = '#FF35B37E'})[self.kind])
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

-- Fires a fast ball from the cannon in the corner toward the pointer, as a bullet that never passes through a thin body.
function Playground:shoot(x, y)
    local fromX, fromY = Playground.cannon[1], Playground.cannon[2]
    local dx, dy = x - fromX, y - fromY
    local length = math.sqrt(dx * dx + dy * dy)
    if length < 1 then
        return
    end
    local speed = self.values.shot
    local ball = self.world:createBody({x = fromX, y = fromY, bullet = true, vx = dx / length * speed, vy = dy / length * speed})
    parts.circle(ball, 12, {density = 4, restitution = 0.2})
    parts.paint(ball, '#FFE5534B')
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
    if self.tool == 'grab' then
        if pointer.pressed then
            self.chosen = self:bodyAt(x, y) or self.chosen
        end
        self.grab:update(pointer, self.camera)
    elseif pointer.pressed then
        if self.tool == 'drop' then
            self:drop(x, y)
        elseif self.tool == 'shoot' then
            self:shoot(x, y)
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
    self:status(string.format('Bodies %d, shots %d, gravity %d across and %d down, step %.2f ms', #self.bodies, #self.shots, self.values.gravityX, self.values.gravityY, self:stepTime()))
end

-- Draws the card of the chosen body beside it, with the values it holds now.
function Playground:drawCard(body)
    local shape = body:shapes()[1]
    local velocity = body.velocity
    local lines = string.format('Mass %.2f kg\nDensity %.1f\nFriction %.2f\nRestitution %.2f\nDamping %.1f, spin %.1f\nGravity scale %.1f\nSpeed %.0f, spin %.1f', body.mass, shape.density, shape.friction, shape.restitution, body.linearDamping, body.angularDamping, body.gravityScale, math.sqrt(velocity.x * velocity.x + velocity.y * velocity.y), body.angularVelocity)
    local x = math.min(560, body.x + 60)
    local y = math.max(-410, math.min(200, body.y - 90))
    graphics2d.drawShape({x, y, 270, 232}, Playground.card)
    graphics2d.drawText(nil, lines, x + 14, y + 12, Playground.cardText)
    graphics2d.drawLine(body.x, body.y, x, y + 116, 2, '#80FFFFFF', {layer = 29})
end

function Playground:draw(area)
    parts.drawAll(self.statics)
    parts.drawAll(self.bodies)
    parts.drawAll(self.shots)
    graphics2d.drawCircle(Playground.cannon[1], Playground.cannon[2], 22, '#FF6B7A8F', {layer = 2})
    if self.tool == 'shoot' then
        graphics2d.drawLine(Playground.cannon[1], Playground.cannon[2], self.pointer.worldX, self.pointer.worldY, 2, '#60E5534B', {layer = 2})
    end
    if self.pushing then
        graphics2d.drawLine(self.pushing[1], self.pushing[2], self.pointer.worldX, self.pointer.worldY, 4, '#FFE8A93A', {layer = 20})
    end
    if self.chosen and self.chosen.valid then
        graphics2d.drawRing(self.chosen.x, self.chosen.y, 52, 3, '#FFFFFFFF', {layer = 20})
        self:drawCard(self.chosen)
    end
    self.grab:draw()
end

return Playground
