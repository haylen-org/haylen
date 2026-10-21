-- State graph: a walker on a meadow whose idle, run and attack clips a state graph picks from its speed, its facing and an attack trigger, with directional states that turn with the stick, the keys or the pointer. The panel draws the graph into a render target that the interface shows, with the state the walker is in lit and the transition that fired glowing, and lists the states it entered. The graph runs in C++ and only the changes of state reach Lua.
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AnimationTest = require('categories.animation.animation-test')
local Backdrop = require('categories.animation.backdrop')
local Meadow = require('categories.animation.meadow')
local Pointer = require('harness.pointer')

local StateGraph = haylen.class('StateGraph', AnimationTest)

StateGraph.lines = 4
StateGraph.speed = 260
StateGraph.scale = 0.85
-- Bushes and rocks of the painted props of the Tiled tests stand around the meadow, at shares of the play area, and sort with the walker by the line they stand on.
StateGraph.props = {
    {source = {266, 266, 128, 128}, x = 0.1, y = 0.28},
    {source = {398, 266, 128, 128}, x = 0.88, y = 0.22},
    {source = {398, 266, 128, 128}, x = 0.14, y = 0.9},
    {source = {266, 266, 128, 128}, x = 0.86, y = 0.86},
}
StateGraph.propSize = 112
-- The feet of the walker stand on this share of the height of its cells.
StateGraph.feet = 248 / 256
StateGraph.image = '@stateGraph'
-- The graph is drawn in a box of 428 by 340 units, into a render target of this many pixels a unit.
StateGraph.box = {428, 340}
StateGraph.density = 2.5
StateGraph.glow = 1.5

StateGraph.actions = {actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left'}},
    {name = 'attack', type = 'button', bindings = {'key:space', 'key:enter', 'button:south'}},
    Pointer.actions[1],
    Pointer.actions[2],
}}

-- The walker faces left and right only, so its up and down clips are its side clips, and the sprite flips for the left ones.
StateGraph.graph = {
    parameters = {speed = 0, attack = 'trigger', facing = {1, 0}},
    states = {
        idle = {directions = {right = 'idleRight', down = 'idleRight', left = 'idleLeft', up = 'idleLeft'}, by = 'facing'},
        run = {directions = {right = 'runRight', down = 'runRight', left = 'runLeft', up = 'runLeft'}, by = 'facing'},
        attack = {animation = 'attack'},
    },
    transitions = {
        {from = 'any', to = 'attack', when = 'attack'},
        {from = 'attack', to = 'idle', atEnd = true},
        {from = 'idle', to = 'run', when = 'speed > 0.2'},
        {from = 'run', to = 'idle', when = 'speed <= 0.2'},
    },
}

-- The states and the transitions as the panel draws them: each transition is a curve from its start to its end, bent toward a control point, with its condition on a chip in the middle.
StateGraph.nodes = {idle = {x = 78, y = 84, text = 'Idle'}, run = {x = 350, y = 84, text = 'Run'}, attack = {x = 214, y = 270, text = 'Attack'}}
StateGraph.nodeSize = {132, 56}
StateGraph.edges = {
    {key = 'idle>run', points = {152, 72, 214, 8, 276, 72}, text = 'Speed > 0.2'},
    {key = 'run>idle', points = {276, 98, 214, 162, 152, 98}, text = 'Speed ≤ 0.2'},
    {key = 'idle>attack', points = {92, 114, 112, 230, 150, 260}, text = 'Attack'},
    {key = 'run>attack', points = {336, 114, 316, 230, 278, 260}, text = 'Attack'},
    {key = 'attack>idle', points = {148, 284, 40, 310, 50, 114}, text = 'At the end'},
}
StateGraph.colors = {
    node = '#FF2F3352', nodeRim = '#FF5D6596', active = '#FF2F86F6', activeRim = '#FFFFC93C', text = '#FFF2F3FF',
    edge = '#FF5D6596', fired = '#FFFFC93C', chip = '#FF1E2138',
}

StateGraph.code = [[
local player = animation2d.newStateGraph(StateGraph.graph):newPlayer(animator)
player:set('speed', length)
player:set('facing', {x, y})
player:trigger('attack')  -- Fires "any > attack" once.
player:on('stateChanged', function(from, to) self:record(from, to) end, {owner = self})
player:update(dt)]]

function StateGraph:enter()
    local sheet = assets.texture('sprites/sheets/walker.png', AnimationTest.linear)
    self.animator = animation2d.newAnimator()
    self.animator.pivotY = StateGraph.feet
    for _, side in ipairs({'Right', 'Left'}) do
        self.animator:add('idle' .. side, animation2d.fromGrid(sheet, {frameWidth = 256, frameHeight = 256, cells = {1, 2, 3, 4}, framesPerSecond = 6}))
        self.animator:add('run' .. side, animation2d.fromGrid(sheet, {frameWidth = 256, frameHeight = 256, cells = {9, 10, 11, 12, 13, 14, 15, 16}, framesPerSecond = 12}))
    end
    self.animator:add('attack', animation2d.fromGrid(sheet, {frameWidth = 256, frameHeight = 256, cells = {17, 18, 19, 20, 21, 22}, framesPerSecond = 12, loop = 'once'}))
    self.player = animation2d.newStateGraph(StateGraph.graph):newPlayer(self.animator)
    self.player:on('stateChanged', function(from, to) self:record(from, to) end, {owner = self})
    self.walker = graphics2d.newSprite(sheet, {scaleX = StateGraph.scale, scaleY = StateGraph.scale})
    self.terrain = assets.texture('tiled/tiles/terrain.png', AnimationTest.linear)
    self.pointer = Pointer()
    self.history = {}
    self.fired = nil
    self.firedTime = 0

    self.target = graphics.newRenderTarget(StateGraph.box[1] * StateGraph.density, StateGraph.box[2] * StateGraph.density, {filter = 'linear'})
    self.targetCamera = graphics2d.newCamera()
    self.targetCamera.position = {StateGraph.box[1] / 2, StateGraph.box[2] / 2}
    self.targetCamera.zoom = {StateGraph.density, StateGraph.density}
    ui.setImage(StateGraph.image, self.target)
    self.edgeColor, self.firedColor = m.color(StateGraph.colors.edge), m.color(StateGraph.colors.fired)
    self.curves, self.chips = {}, {}
    for _, edge in ipairs(StateGraph.edges) do
        local p = edge.points
        self.curves[edge.key] = m.spline({{p[1], p[2]}, {p[1] + (p[3] - p[1]) * 2 / 3, p[2] + (p[4] - p[2]) * 2 / 3}, {p[5] + (p[3] - p[5]) * 2 / 3, p[6] + (p[4] - p[6]) * 2 / 3}, {p[5], p[6]}}, {kind = 'bezier'})
        self.chips[edge.key] = graphics2d.measureText(nil, edge.text, {size = 22}) + 22
    end

    self:loadActions(StateGraph.actions)
    self:frame{
        code = StateGraph.code,
        play = true,
        hint = 'Move with the keys, the d-pad, the stick or a press on the meadow, and attack with Space, Enter, the south button or Attack.',
        controls = {
            ui.button{id = 'attack', text = 'Attack', variant = 'primary', onClick = function() self.player:trigger('attack') end},
            ui.image{image = StateGraph.image, width = StateGraph.box[1], height = StateGraph.box[2], align = 'center', radius = 12},
            ui.sectionTitle{text = 'States entered'},
            ui.column{id = 'history', gap = 4, ui.label{text = 'The walker stands in the idle state.', color = 'textMuted'}},
        },
    }
end

function StateGraph:exit()
    StateGraph.super.exit(self)
    ui.removeImage(StateGraph.image)
end

function StateGraph:record(from, to)
    self.fired = from .. '>' .. to
    self.firedTime = StateGraph.glow
    table.insert(self.history, 1, string.format('%s to %s', StateGraph.nodes[from].text, StateGraph.nodes[to].text:lower()))
    self.history[StateGraph.lines + 1] = nil
    local rows = {}
    for index, text in ipairs(self.history) do
        rows[index] = ui.label{text = text, color = index == 1 and 'text' or 'textMuted'}
    end
    self.gui:replaceChildren('history', rows)
end

-- The walker starts in the middle of the meadow and stays inside it.
function StateGraph:resize(area)
    self.meadow = Meadow(area, 96)
    self.x = self.x and m.clamp(self.x, 48, area.width - 48) or area.width * 0.5
    self.y = self.y and m.clamp(self.y, 0, area.height) or area.height * 0.62
end

function StateGraph:update(dt)
    StateGraph.super.update(self, dt)
    if not self.area then
        return
    end
    self.pointer:update(dt, self)

    -- A press on the meadow walks the walker toward it, and the keys, the d-pad and the stick steer it.
    local dx, dy = input.vector('move')
    if self.pointer.down then
        local toX, toY = self.pointer.worldX - self.x, self.pointer.worldY - self.y
        local distance = math.sqrt(toX * toX + toY * toY)
        if distance > 12 then
            dx, dy = toX / distance, toY / distance
        end
    end
    local length = math.min(1, math.sqrt(dx * dx + dy * dy))
    self.player:set('speed', length)
    if length > 0 then
        self.player:set('facing', {dx, dy})
    end
    if input.pressed('attack') then
        self.player:trigger('attack')
    end
    self.player:update(dt)
    self.animator:update(dt)
    self.firedTime = math.max(0, self.firedTime - dt)

    local top = self.walker.scaleY * 256 * StateGraph.feet * 0.8
    if self.player.state == 'run' then
        self.x = m.clamp(self.x + dx * StateGraph.speed * dt, 48, self.area.width - 48)
        self.y = m.clamp(self.y + dy * StateGraph.speed * dt, top, self.area.height - 12)
    end
    self.walker.x, self.walker.y = self.x, self.y
    self.walker.flipHorizontal = self.player.animation:find('Left') ~= nil or (self.player.state == 'attack' and self.player:get('facing').x < 0)
    self.animator:apply(self.walker)
    self:status(self:describe(length))
end

function StateGraph:describe(length)
    local side = self.walker.flipHorizontal and 'left' or 'right'
    local seconds = self.player.stateTime
    if self.player.state == 'run' then
        return string.format('Running to the %s at %d percent of full speed for %.1f seconds.', side, math.floor(length * 100 + 0.5), seconds)
    elseif self.player.state == 'attack' then
        return string.format('Swinging the sword to the %s for %.1f seconds.', side, seconds)
    end
    return string.format('Standing and facing %s for %.1f seconds.', side, seconds)
end

function StateGraph:draw(area)
    self.meadow:draw()
    for _, prop in ipairs(StateGraph.props) do
        local x, y = area.width * prop.x, area.height * prop.y
        local layer = y <= self.walker.y and -1 or 1
        graphics2d.draw(self.terrain, x, y, {source = prop.source, width = StateGraph.propSize, height = StateGraph.propSize, pivotY = 0.85, layer = layer})
    end
    Backdrop.shadow(self.walker.x, self.walker.y, 130, -2)
    self.walker:draw()
end

function StateGraph:render()
    StateGraph.super.render(self)
    graphics2d.beginTarget(self.target, self.targetCamera, {clear = ui.themeColor('window')})
    for _, edge in ipairs(StateGraph.edges) do
        self:drawEdge(edge)
    end
    for name, node in pairs(StateGraph.nodes) do
        self:drawNode(name, node)
    end
end

-- A transition that has just fired glows in the color of the focus over its own color, and the glow fades.
function StateGraph:drawEdge(edge)
    local glow = edge.key == self.fired and self.firedTime / StateGraph.glow or 0
    self:drawTransition(edge, self.edgeColor, 3, 1)
    if glow > 0 then
        self:drawTransition(edge, self.firedColor:withAlpha(glow), 6, 3)
    end
    local p = edge.points
    local middleX, middleY = (p[1] + p[3] * 2 + p[5]) / 4, (p[2] + p[4] * 2 + p[6]) / 4
    local width = self.chips[edge.key]
    graphics2d.drawShape({middleX - width / 2, middleY - 16, width, 32}, {radius = 16, color = StateGraph.colors.chip, borderWidth = 2, borderColor = glow > 0.5 and StateGraph.colors.fired or StateGraph.colors.edge, layer = 5})
    graphics2d.drawText(nil, edge.text, middleX, middleY, {size = 22, color = StateGraph.colors.text, anchor = {0.5, 0.5}, layer = 6})
end

-- The curve of a transition with the arrow that points along it where it reaches its end.
function StateGraph:drawTransition(edge, color, width, layer)
    local p = edge.points
    graphics2d.drawStroke(self.curves[edge.key], {width = width, cap = 'round', color = color, layer = layer})
    local dx, dy = p[5] - p[3], p[6] - p[4]
    local length = math.sqrt(dx * dx + dy * dy)
    dx, dy = dx / length, dy / length
    local size = 12 + width
    graphics2d.drawPolygon({{p[5], p[6]}, {p[5] - dx * size - dy * size * 0.6, p[6] - dy * size + dx * size * 0.6}, {p[5] - dx * size + dy * size * 0.6, p[6] - dy * size - dx * size * 0.6}}, color, {layer = layer + 1})
end

-- The state the walker is in is lit, and its directional states show the side they face.
function StateGraph:drawNode(name, node)
    local colors = StateGraph.colors
    local active = self.player.state == name
    local width, height = StateGraph.nodeSize[1], StateGraph.nodeSize[2]
    local rect = {node.x - width / 2, node.y - height / 2, width, height}
    if active then
        graphics2d.drawShape({rect[1] - 8, rect[2] - 8, width + 16, height + 16}, {radius = height / 2 + 8, color = '#802F86F6', softness = 14, layer = 7})
    end
    graphics2d.drawShape(rect, {radius = height / 2, color = active and colors.active or colors.node, borderWidth = 3, borderColor = active and colors.activeRim or colors.nodeRim, layer = 8})
    graphics2d.drawText(nil, node.text, node.x, node.y, {size = 26, color = colors.text, anchor = {0.5, 0.5}, layer = 9})
    if active and name ~= 'attack' then
        local side = self.walker.flipHorizontal and -1 or 1
        local tipX = node.x + side * (width / 2 - 12)
        graphics2d.drawPolygon({{tipX, node.y}, {tipX - side * 9, node.y - 7}, {tipX - side * 9, node.y + 7}}, colors.text, {layer = 9})
    end
end

function StateGraph:renderUi()
    self.pointer:draw()
end

return StateGraph
