-- Folds and dual screens: a harbor map and its instruments laid out around the fold, on either side of a book or a hinge and above and below a tabletop, and together when nothing separates the window. The buttons simulate each posture on any screen, and "Device" shows the fold that the device reports, which Android does through Jetpack WindowManager and browsers through the Viewport Segments API.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Test = require('harness.test')

local Folds = haylen.class('Folds', Test)

local kSimulations = {
    {id = 'device', text = 'The fold of the device'},
    {id = 'book', text = 'Book'},
    {id = 'tabletop', text = 'Tabletop'},
    {id = 'flat', text = 'Opened flat'},
    {id = 'dualScreen', text = 'Dual screen with a hinge'},
}
local kSea = '#FF1B3A57'
local kLand = '#FF5E8C4A'
local kInstruments = '#FF20242F'
local kIslands = {{0.25, 0.3, 0.12}, {0.7, 0.62, 0.16}, {0.4, 0.78, 0.08}}

function Folds:enter()
    self.previous = window.foldSimulation()
    self.changes = 0
    self.time = 0
    self:listen('windowFoldChanged', function(change)
        self.changes = self.changes + 1
        self:log('The fold changed to the posture "%s" with %d segments.', change.posture, #change.segments)
    end)

    local controls = {ui.label{text = 'Simulate a fold', font = 'heading'}}
    for _, simulation in ipairs(kSimulations) do
        controls[#controls + 1] = ui.button{id = simulation.id, text = simulation.text, align = 'stretch', onClick = function()
            window.setFoldSimulation(simulation.id ~= 'device' and simulation.id or nil)
        end}
    end
    self:frame{
        hint = 'The map and the instruments move to each side of the fold. Fold or turn a foldable device, or pick a simulated fold.',
        controls = controls,
        focus = 'book',
    }
end

function Folds:exit()
    window.setFoldSimulation(self.previous)
    Folds.super.exit(self)
end

-- The parts of the play area on each side of the fold, in the coordinates of the stage, and the fold itself where it crosses the play area.
function Folds:regions()
    local stage = self.stage
    local parts = {}
    for _, segment in ipairs(window.segments()) do
        local part = segment:intersection(stage)
        if part.width > 1 and part.height > 1 then
            parts[#parts + 1] = m.rect(part.x - stage.x, part.y - stage.y, part.width, part.height)
        end
    end
    local fold = window.fold()
    local crossing = fold and fold.bounds:expanded(1):intersection(stage)
    if crossing and crossing.width > 0 and crossing.height > 0 then
        crossing = m.rect(crossing.x - stage.x, crossing.y - stage.y, crossing.width, crossing.height)
    else
        crossing = nil
    end
    return parts, crossing, fold
end

function Folds:update(dt)
    Folds.super.update(self, dt)
    self.time = self.time + dt
    if not self.stage then
        return
    end
    local fold = window.fold()
    local where = fold and string.format('A %s fold, "%s"%s, at %s.', fold.axis, fold.state, fold.occluding and ' with a hinge' or '', tostring(fold.bounds)) or 'No fold crosses the window.'
    self:status(string.format('Posture "%s" with %d segments. %s Changes %d.', window.posture(), #window.segments(), where, self.changes))
end

function Folds:draw(area)
    local parts, crossing, fold = self:regions()

    -- One region holds the map with the instruments in a corner, and two regions give the map the first one and the instruments the second.
    if #parts >= 2 then
        self:drawMap(parts[1])
        self:drawInstruments(parts[2])
    else
        local whole = parts[1] or area
        self:drawMap(whole)
        local size = math.min(whole.width, whole.height) * 0.42
        self:drawInstruments(m.rect(whole:right() - size - 24, whole:bottom() - size - 24, size, size))
    end
    if crossing then
        self:drawFold(crossing, fold)
    end
end

function Folds:drawMap(region)
    graphics2d.drawRect(region, kSea)
    for column = 1, 7 do
        local x = region.x + region.width * column / 8
        graphics2d.drawLine(x, region.y, x, region:bottom(), 1, '#40FFFFFF', {layer = 1})
    end
    for row = 1, 5 do
        local y = region.y + region.height * row / 6
        graphics2d.drawLine(region.x, y, region:right(), y, 1, '#40FFFFFF', {layer = 1})
    end
    local scale = math.min(region.width, region.height)
    for _, island in ipairs(kIslands) do
        graphics2d.drawCircle(region.x + region.width * island[1], region.y + region.height * island[2], scale * island[3], kLand, {layer = 2})
    end

    -- The boat sails a loop around the middle island.
    local x = region.x + region.width * (0.55 + 0.3 * math.cos(self.time * 0.5))
    local y = region.y + region.height * (0.5 + 0.3 * math.sin(self.time * 0.5))
    graphics2d.drawCircle(x, y, math.max(8, scale * 0.03), Test.warm, {layer = 3})
    Test.caption('Harbor map', region.x + 16, region.y + 12, {size = 26, color = Test.ink, layer = 4})
end

function Folds:drawInstruments(region)
    graphics2d.drawRect(region, kInstruments, {layer = 5})
    graphics2d.drawRectOutline(region, 2, Test.line, {layer = 6})
    local radius = math.min(region.width, region.height) * 0.3
    local x, y = region.x + region.width / 2, region.y + region.height / 2
    graphics2d.drawRing(x, y, radius, 4, Test.accent, {layer = 6})
    local heading = self.time * 0.5 + math.pi
    graphics2d.drawLine(x, y, x + math.cos(heading) * radius * 0.9, y + math.sin(heading) * radius * 0.9, 6, Test.red, {layer = 7})
    Test.caption('Instruments', region.x + 16, region.y + 12, {size = 26, color = Test.ink, layer = 7})
    Test.caption('Heading ' .. math.floor(math.deg(heading) % 360) .. ' degrees', x, y + radius + 20, {anchor = {0.5, 0}, layer = 7})
end

-- A hinge hides what lies under it, and a seamless fold is a line the content keeps away from.
function Folds:drawFold(bounds, fold)
    if fold.occluding then
        graphics2d.drawRect(bounds, '#FF050608', {layer = 8})
        Test.caption('Hinge', bounds.x + bounds.width / 2, bounds.y + bounds.height / 2, {anchor = {0.5, 0.5}, color = Test.muted, layer = 9})
        return
    end
    local center = bounds:center()
    if fold.axis == 'vertical' then
        graphics2d.drawLine(center.x, bounds.y, center.x, bounds:bottom(), 3, Test.violet, {layer = 8})
    else
        graphics2d.drawLine(bounds.x, center.y, bounds:right(), center.y, 3, Test.violet, {layer = 8})
    end
end

return Folds
