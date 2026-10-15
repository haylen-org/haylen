-- State graph: a walker whose idle, run and attack clips a state graph picks from its speed, its facing and an attack trigger, with directional states that turn with the stick or the keys and a list of the states it entered. The graph runs in C++ and only the changes of state reach Lua.
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local AnimationTest = require('categories.animation.animation-test')

local StateGraph = haylen.class('StateGraph', AnimationTest)

StateGraph.lines = 6
StateGraph.speed = 260

StateGraph.actions = {actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left'}},
    {name = 'attack', type = 'button', bindings = {'key:space', 'key:enter', 'button:south'}},
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

StateGraph.code = [[
local player = animation2d.newStateGraph(graph):newPlayer(animator)
player:set('speed', length)  player:set('facing', {x, y})
player:trigger('attack')  -- Fires "any > attack" once.
player:update(dt)  animator:update(dt)  -- Only state changes reach Lua.]]

function StateGraph:enter()
    local sheet = assets.texture('sprites/sheets/walker.png', {filter = 'linear'})
    self.animator = animation2d.newAnimator()
    self.animator.pivotY = 0.9
    for _, side in ipairs({'Right', 'Left'}) do
        self.animator:add('idle' .. side, animation2d.fromGrid(sheet, {frameWidth = 128, frameHeight = 128, cells = {1, 2, 3, 4}, framesPerSecond = 6}))
        self.animator:add('run' .. side, animation2d.fromGrid(sheet, {frameWidth = 128, frameHeight = 128, cells = {9, 10, 11, 12, 13, 14, 15, 16}, framesPerSecond = 12}))
    end
    self.animator:add('attack', animation2d.fromGrid(sheet, {frameWidth = 128, frameHeight = 128, cells = {17, 18, 19, 20, 21, 22}, framesPerSecond = 12, loop = 'once'}))
    self.player = animation2d.newStateGraph(StateGraph.graph):newPlayer(self.animator)
    self.player:on('stateChanged', function(from, to) self:record(string.format('From "%s" to "%s"', from, to)) end, {owner = self})
    self.walker = graphics2d.newSprite(sheet, {scaleX = 1.5, scaleY = 1.5})
    self.history = {}
    self:loadActions(StateGraph.actions)
    self:frame{
        code = StateGraph.code,
        play = true,
        hint = 'Move with the keys, the d-pad or the stick, and attack with Space, Enter, the south button or the Attack button.',
        controls = {
            ui.button{id = 'attack', text = 'Attack', variant = 'primary', onClick = function() self.player:trigger('attack') end},
            ui.column{id = 'history', gap = 4},
        },
    }
end

function StateGraph:record(line)
    table.insert(self.history, 1, line)
    self.history[StateGraph.lines + 1] = nil
    local rows = {}
    for index, text in ipairs(self.history) do
        rows[index] = ui.label{text = text, font = 'caption', color = index == 1 and 'text' or 'textMuted'}
    end
    self.gui:replaceChildren('history', rows)
end

-- The walker starts in the middle of the play area and stays inside it.
function StateGraph:resize(area)
    self.x = self.x and math.min(self.x, area.width - 48) or area.width * 0.5
    self.y = self.y and math.min(self.y, area.height - 20) or area.height * 0.6
end

function StateGraph:update(dt)
    StateGraph.super.update(self, dt)
    if not self.area then
        return
    end
    local dx, dy = input.vector('move')
    local length = math.sqrt(dx * dx + dy * dy)
    self.player:set('speed', length)
    if length > 0 then
        self.player:set('facing', {dx, dy})
    end
    if input.pressed('attack') then
        self.player:trigger('attack')
    end
    self.player:update(dt)
    self.animator:update(dt)

    if self.player.state == 'run' then
        self.x = math.max(48, math.min(self.area.width - 48, self.x + dx * StateGraph.speed * dt))
        self.y = math.max(120, math.min(self.area.height - 20, self.y + dy * StateGraph.speed * dt))
    end
    self.walker.x = self.x
    self.walker.y = self.y
    self.walker.flipHorizontal = self.player.animation:find('Left') ~= nil or (self.player.state == 'attack' and self.player:get('facing').x < 0)
    self.animator:apply(self.walker)
    self:status(string.format('State %s   Animation %s   Time in state %.2f   Speed %.2f', self.player.state, self.player.animation, self.player.stateTime, length))
end

function StateGraph:draw(area)
    self.walker:draw()
end

return StateGraph
