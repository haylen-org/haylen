-- Guards run by behavior trees: they chase the player when they see it past the walls, rest when their energy runs low and patrol their posts otherwise.
local ai = require('haylen.ai')
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')

local BehaviorTree = haylen.class('BehaviorTree', AlgorithmTest)

BehaviorTree.code = [[
local guard = ai.newBehaviorTree(ai.selector({
    ai.sequence({ai.condition(seesPlayer), ai.action(chase), ai.succeeder(ai.cooldown(ai.action(shout), 2))}),
    ai.sequence({ai.condition(tired), ai.action(rest)}),
    ai.action(patrol),
}), {energy = 1, shouts = 0})
guard:tick(dt)  -- Starts over from the root once a branch finishes.]]

local kSight = 300
local kSpeed = 150
local kWalls = {{{-300, -250}, {-300, 100}}, {{100, -100}, {400, -100}}, {{0, 150}, {0, 380}}, {{-600, 0}, {-420, 0}}, {{450, 100}, {650, 250}}}
local kRest = {{-650, -330}, {650, 330}}
local kArena = {-780, -410, 1560, 820}

local function moveToward(board, target, dt, speed)
    local offset = m.vec2(target[1], target[2]) - board.position
    local distance = offset:length()
    local step = speed * dt
    if distance <= step then
        board.position = m.vec2(target[1], target[2])
        return true
    end
    board.position = board.position + offset / distance * step
    board.heading = offset:angle()
    return false
end

-- Sight needs the player in range and no wall on the segment between them.
local function sees(board)
    local player = board.player
    if player == nil or board.position:distance(player) > kSight then
        return false
    end
    return m.raycastSegments(board.position, player, kWalls) == nil
end

-- Every leaf finishes within its tick, so the tree starts over from the root each tick and a guard notices the intruder at once.
local function tree()
    return ai.selector({
        ai.sequence({
            ai.condition(sees),
            ai.action(function(board, dt)
                board.state = 'Chasing'
                board.energy = math.max(0, board.energy - dt * 0.12)
                moveToward(board, {board.player.x, board.player.y}, dt, kSpeed * 1.5)
            end),
            ai.succeeder(ai.cooldown(ai.action(function(board) board.shouts = board.shouts + 1 end), 2)),
        }),
        ai.sequence({
            ai.condition(function(board)
                board.resting = board.resting or board.energy < 0.3
                return board.resting
            end),
            ai.action(function(board, dt)
                if moveToward(board, board.bed, dt, kSpeed) then
                    board.state = 'Resting'
                    board.energy = math.min(1, board.energy + dt * 0.35)
                    board.resting = board.energy < 1
                else
                    board.state = 'Going to rest'
                end
            end),
        }),
        ai.sequence({
            ai.condition(function(board) return board.pause > 0 end),
            ai.action(function(board, dt)
                board.state = 'Looking around'
                board.pause = board.pause - dt
                board.heading = board.heading + dt * 2
            end),
        }),
        ai.action(function(board, dt)
            board.state = 'Patrolling'
            board.energy = math.max(0, board.energy - dt * 0.05)
            if moveToward(board, board.posts[board.post], dt, kSpeed) then
                board.post = board.post % #board.posts + 1
                board.pause = 0.8
            end
        end),
    })
end

function BehaviorTree:enter()
    self:frame{
        hint = 'The intruder in red follows the pointer. Each guard ticks its own tree, patrols its posts, chases the intruder it sees past the walls, shouting at most every two seconds, and rests at a campfire when its energy runs low. R or the X button resets the trees.',
        code = BehaviorTree.code,
        controls = {
            ui.button{id = 'tired', text = 'Tire every guard', onClick = function() self:tire() end},
            ui.button{id = 'reset', text = 'Reset the trees', onClick = function() self:build() end},
            ui.formField{label = 'What each guard does', ui.label{id = 'states', text = '', font = 'monospace'}},
        },
        focus = 'tired',
    }
    self.art = Art()
    self.campfire = assets.texture('effects/campfire.png', {filter = 'linear'})
    local glow = graphics.newImage(128, 128)
    glow:fillGradient({kind = 'radial', stops = {{0, '#FFFFFFFF'}, {1, '#00FFFFFF'}}})
    self.glow = graphics.newTexture(glow, {filter = 'linear'})
    self:build()
end

function BehaviorTree:build()
    local posts = {
        {{-500, -300}, {-500, 250}, {-350, 250}},
        {{-150, -330}, {250, -330}, {250, -180}},
        {{150, 50}, {350, 300}, {150, 320}},
        {{500, -300}, {680, -60}, {520, 20}},
    }
    self.guards = {}
    for index, route in ipairs(posts) do
        local board = {position = m.vec2(route[1][1], route[1][2]), heading = 0, posts = route, post = 2, pause = 0, shouts = 0, resting = false, energy = 0.5 + index * 0.12, bed = kRest[index % 2 + 1], state = ''}
        self.guards[index] = ai.newBehaviorTree(tree(), board)
    end
end

function BehaviorTree:tire()
    for _, guard in ipairs(self.guards) do
        guard.blackboard.energy = 0.1
    end
end

function BehaviorTree:update(dt)
    BehaviorTree.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    -- The intruder waits in the middle until the pointer comes over the play area.
    self.intruder = m.vec2(self:aim(-60, 20))
    local lines = {'Guard  State           Energy  Shouts'}
    for index, guard in ipairs(self.guards) do
        guard.blackboard.player = self.intruder
        guard:tick(dt)
        local board = guard.blackboard
        lines[index + 1] = string.format('%-6d %-15s %5d%%  %6d', index, board.state, math.floor(board.energy * 100), board.shouts)
    end
    local states = table.concat(lines, '\n')
    if states ~= self.states then
        self.states = states
        self:set('states', {text = states})
    end
    self:status(string.format('%d trees of %d nodes each', #self.guards, self.guards[1].nodeCount))
end

-- The guards walk a courtyard with walls and two campfires to rest at, each with a cone of sight that fades with distance and turns red while it chases.
function BehaviorTree:draw(area)
    graphics2d.drawShape(kArena, {radius = 18, color = '#FF26303F', borderWidth = 4, borderColor = '#FF3A4560', layer = -2})
    for _, wall in ipairs(kWalls) do
        graphics2d.drawStroke({wall[1][1], wall[1][2], wall[2][1], wall[2][2]}, {width = 16, color = '#FF15172A', cap = 'round', layer = 1})
        graphics2d.drawStroke({wall[1][1], wall[1][2], wall[2][1], wall[2][2]}, {width = 10, color = '#FF8D97B5', cap = 'round', layer = 1})
    end
    for _, bed in ipairs(kRest) do
        graphics2d.draw(self.campfire, bed[1], bed[2] + 24, {width = 120, height = 75, pivotY = 1, layer = BehaviorTree.depth(bed[2])})
        self.art:label('Rest spot', bed[1], bed[2] + 44, 18, '#FFE8EAF2', 1000)
    end
    graphics2d.pushClip(kArena)
    for _, guard in ipairs(self.guards) do
        local board = guard.blackboard
        local x, y = board.position.x, board.position.y
        local chasing = board.state == 'Chasing'
        graphics2d.drawShape({x - kSight, y - kSight, kSight * 2, kSight * 2}, {kind = 'ellipse', startAngle = board.heading - 0.6, sweep = 1.2, texture = self.glow, color = chasing and '#70F0473E' or '#38FFFFFF', layer = 0})
    end
    graphics2d.popClip()
    for _, guard in ipairs(self.guards) do
        local board = guard.blackboard
        local x, y = board.position.x, board.position.y
        local depth = BehaviorTree.depth(y)
        Art.shadow(x, y, 46, depth - 1)
        self.art:character(x, y + 6, 104, board.state == 'Resting' and 'violet' or 'blue', depth, math.cos(board.heading) < 0)
        graphics2d.drawShape({x - 30, y + 14, 60, 9}, {radius = 4.5, color = '#D0151824', layer = 1001})
        graphics2d.drawShape({x - 30, y + 14, math.max(9, 60 * board.energy), 9}, {radius = 4.5, color = '#FF6FDCA0', layer = 1002})
        self.art:label(board.state, x, y + 44, 18, board.state == 'Chasing' and '#FFFF8A84' or '#FFE8EAF2', 1003)
    end
    local intruder = self.intruder
    Art.shadow(intruder.x, intruder.y, 46, BehaviorTree.depth(intruder.y) - 1)
    self.art:character(intruder.x, intruder.y + 6, 104, 'red', BehaviorTree.depth(intruder.y))
end

-- Things lower in the courtyard draw in front of the ones behind them.
function BehaviorTree.depth(y)
    return math.floor(y + 500)
end

return BehaviorTree
