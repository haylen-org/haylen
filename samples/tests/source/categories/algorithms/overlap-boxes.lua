-- Overlap boxes without physics: fighters on a belt of depth rows whose punches reach the bodies of the other team only within the depth band, placed and read through float buffers in two calls per step, with a rest after every hit and the boxes of the overlap world drawn over them.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local spatial2d = require('haylen.spatial2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')

local OverlapBoxes = haylen.class('OverlapBoxes', AlgorithmTest)

local kFighters = 60
local kMaxContacts = 256
local kRest = 20
local kTeams = {{layers = 1, mask = 2, color = '#FF8FB0FF'}, {layers = 2, mask = 1, color = '#FFF2B23A'}}

function OverlapBoxes:enter()
    self:frame{
        hint = 'Fighters of two teams wander a belt of depth rows and punch. A punch lands only on the other team within the depth band, and a pair rests after each hit.',
        controls = {
            ui.toggle{id = 'boxes', text = 'Draw the boxes', checked = true, onChange = function(event) self.drawBoxes = event.checked end},
            ui.formField{label = 'Depth reach of a body', ui.slider{id = 'reach', min = 0, max = 60, step = 2, value = 12, showValue = true, onChange = function(event) self:defineSets(event.value) end}},
            ui.label{font = 'monospace', text = "world:placeAll(placements)\nworld:contacts(contacts)\nworld:rest(source, target, ticks)"},
        },
    }
    self.drawBoxes = true
    self.random = m.random(11)
    self.world = spatial2d.newOverlapWorld({kinds = {'body', 'hit'}, cellSize = 160})
    self.placements = collections.newFloatBuffer(kFighters * 6)
    self.contacts = collections.newFloatBuffer(kMaxContacts * 8)
    self.fighters = {}
    self.values = {}
    for index = 1, kFighters do
        self.fighters[index] = {id = self.world:add(), team = index % 2 + 1, x = self.random:range(-720, 720), depth = self.random:range(-300, 340), facing = 1, punch = 0, flash = 0, vx = self.random:range(-80, 80), vy = self.random:range(-30, 30)}
    end
    self.hits = 0
    self:defineSets(12)
end

-- Each team has a standing set and a punching set, and a new reach of the bodies defines them again.
function OverlapBoxes:defineSets(reach)
    self.sets = {}
    for team, rule in ipairs(kTeams) do
        local body = {kind = 'body', x = -18, y = -78, width = 36, height = 78, layers = rule.layers, depth = reach}
        self.sets[team] = {
            standing = self.world:defineSet({body}),
            punching = self.world:defineSet({body, {kind = 'hit', x = 12, y = -62, width = 40, height = 14, layers = 0, mask = rule.mask}}),
        }
    end
end

function OverlapBoxes:fixedUpdate(step)
    local values = self.values
    for index, fighter in ipairs(self.fighters) do
        fighter.x = fighter.x + fighter.vx * step
        fighter.depth = fighter.depth + fighter.vy * step
        if math.abs(fighter.x) > 740 then
            fighter.vx = -fighter.vx
        end
        if fighter.depth < -320 or fighter.depth > 360 then
            fighter.vy = -fighter.vy
        end
        fighter.facing = fighter.vx < 0 and -1 or 1
        fighter.punch = fighter.punch > 0 and fighter.punch - 1 or (self.random:chance(0.02) and 12 or 0)
        fighter.flash = math.max(0, fighter.flash - step)
        local sets = self.sets[fighter.team]
        local base = (index - 1) * 6
        values[base + 1], values[base + 2], values[base + 3] = fighter.id, fighter.x, fighter.depth
        values[base + 4], values[base + 5], values[base + 6] = fighter.depth, fighter.facing, fighter.punch > 0 and sets.punching or sets.standing
    end
    self.placements:set(1, values)
    self.world:placeAll(self.placements)

    local count = self.world:contacts(self.contacts)
    for index = 0, math.min(count, kMaxContacts) - 1 do
        local base = index * 8
        local source, target = self.contacts[base + 1], self.contacts[base + 3]
        self.world:rest(source, target, kRest)
        self.fighters[target].flash = 0.3
        self.hits = self.hits + 1
    end
    self.world:step()
end

function OverlapBoxes:update(dt)
    OverlapBoxes.super.update(self, dt)
    self:status(string.format('Fighters %d, hits %d', kFighters, self.hits))
end

function OverlapBoxes:draw(area)
    for _, fighter in ipairs(self.fighters) do
        local color = fighter.flash > 0 and '#FFFFFFFF' or kTeams[fighter.team].color
        graphics2d.drawRect({fighter.x - 16, fighter.depth - 76, 32, 76}, color, {layer = 1, sortOffset = fighter.depth})
        if fighter.punch > 0 then
            graphics2d.drawRect({fighter.x + (fighter.facing > 0 and 12 or -52), fighter.depth - 60, 40, 10}, color, {layer = 1})
        end
    end
    if self.drawBoxes then
        self.world:debugDraw({layer = 10})
    end
end

return OverlapBoxes
