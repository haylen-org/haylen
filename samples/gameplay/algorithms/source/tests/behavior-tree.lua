-- Guards run by behavior trees: they chase the player when they see it past the walls, rest when their energy runs low and patrol their posts otherwise.
local haylen = require('haylen')
local ai = require('haylen.ai')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local sample = require('sample')

local BehaviorTree = haylen.class('BehaviorTree', sample.Test)

local kSight = 300
local kSpeed = 150
local kWalls = {{{-300, -250}, {-300, 100}}, {{100, -100}, {400, -100}}, {{0, 150}, {0, 380}}, {{-600, 0}, {-420, 0}}, {{450, 100}, {650, 250}}}
local kRest = {{-650, -330}, {650, 330}}

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
                board.state = 'chasing'
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
                    board.state = 'resting'
                    board.energy = math.min(1, board.energy + dt * 0.35)
                    board.resting = board.energy < 1
                else
                    board.state = 'going to rest'
                end
            end),
        }),
        ai.sequence({
            ai.condition(function(board) return board.pause > 0 end),
            ai.action(function(board, dt)
                board.state = 'looking around'
                board.pause = board.pause - dt
                board.heading = board.heading + dt * 2
            end),
        }),
        ai.action(function(board, dt)
            board.state = 'patrolling'
            board.energy = math.max(0, board.energy - dt * 0.05)
            if moveToward(board, board.posts[board.post], dt, kSpeed) then
                board.post = board.post % #board.posts + 1
                board.pause = 0.8
            end
        end),
    })
end

function BehaviorTree:enter()
    BehaviorTree.super.enter(self, {
        hint = 'The white dot follows the pointer and plays the intruder. Each guard ticks its own tree, and a guard that chases shouts at most every two seconds.',
        controls = {
            ui.button{id = 'tired', text = 'Tire every guard', onClick = function() self:tire() end},
            ui.button{id = 'reset', text = 'Reset the trees', onClick = function() self:build() end},
            ui.label{id = 'states', text = '', font = 'caption', color = 'textMuted'},
        },
        stats = true,
        focus = 'tired',
    })
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

function BehaviorTree:exit()
    BehaviorTree.super.exit(self)
    self.guards = nil
end

function BehaviorTree:update(dt)
    BehaviorTree.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local player = m.vec2(self.pointer.worldX, self.pointer.worldY)
    local lines = {}
    for index, guard in ipairs(self.guards) do
        guard.blackboard.player = player
        guard:tick(dt)
        local board = guard.blackboard
        lines[index] = string.format('guard %d: %s, energy %d%%, shouts %d', index, board.state, math.floor(board.energy * 100), board.shouts)
    end
    local states = table.concat(lines, '\n')
    if states ~= self.states then
        self.states = states
        self:set('states', {text = states})
    end
    self:showStats(string.format('trees %d\nnodes per tree %d', #self.guards, self.guards[1].nodeCount))
end

function BehaviorTree:render()
    self:beginWorld()
    for _, wall in ipairs(kWalls) do
        graphics2d.drawLine(wall[1][1], wall[1][2], wall[2][1], wall[2][2], 8, '#FF78909C')
    end
    for _, bed in ipairs(kRest) do
        graphics2d.drawRect({bed[1] - 40, bed[2] - 25, 80, 50}, '#FF5D4037')
    end
    for _, guard in ipairs(self.guards) do
        local board = guard.blackboard
        local x, y = board.position.x, board.position.y
        local chasing = board.state == 'chasing'
        graphics2d.drawArc(x, y, kSight / 2, kSight, board.heading - 0.6, board.heading + 0.6, chasing and '#22EF5350' or '#16FFFFFF')
        graphics2d.drawCircle(x, y, 18, chasing and '#FFEF5350' or (board.state == 'resting' and '#FF7E57C2' or '#FF42A5F5'), {layer = 1})
        graphics2d.drawRect({x - 20, y - 34, 40 * board.energy, 6}, '#FF66BB6A', {layer = 1})
        graphics2d.drawText(nil, board.state, x, y + 24, {size = 20, anchor = {0.5, 0}, layer = 2})
    end
    graphics2d.drawCircle(self.pointer.worldX, self.pointer.worldY, 10, '#FFFFFFFF', {layer = 3})
end

return BehaviorTree
