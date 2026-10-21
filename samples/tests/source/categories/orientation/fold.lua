-- Folds and dual screens: a harbor map and its instruments laid out around the fold, on either side of a book or a hinge and above and below a tabletop, and together when nothing separates the window. The radio buttons simulate each posture on any screen, and the fold of the device shows the fold that the device reports, which Android does through Jetpack WindowManager and browsers through the Viewport Segments API.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Test = require('harness.test')

local Folds = haylen.class('Folds', Test)

Folds.code = [[
local segments = window.segments()  -- Two areas when a fold separates the window.
local map, instruments = segments[1], segments[2] or segments[1]
local fold = window.fold()  -- With bounds, axis, state, separating and occluding.
events.on('windowFoldChanged', function(change) print(change.posture, #change.segments) end, {owner = self})
window.setFoldSimulation('book')  -- Or `tabletop`, `flat`, `dualScreen` and nil for the device.]]

Folds.simulations = {
    {id = 'device', text = 'The fold of the device'},
    {id = 'book', text = 'Book'},
    {id = 'tabletop', text = 'Tabletop'},
    {id = 'flat', text = 'Opened flat'},
    {id = 'dualScreen', text = 'Dual screen with a hinge'},
}
Folds.postures = {book = 'held like a book', tabletop = 'standing like a laptop on a table', flat = 'opened flat'}
Folds.directions = {'north', 'northeast', 'east', 'southeast', 'south', 'southwest', 'west', 'northwest'}
Folds.outline = '#FF1E1B33'

function Folds:enter()
    self.previous = window.foldSimulation()
    self.changes = 0
    self.time = 0
    self.map = assets.texture('interface/images/island-map.png', {filter = 'linear'})
    self.boat = assets.texture('interface/images/boat-top.png', {filter = 'linear'})
    -- The test opens on a book, so a fold shows from the first frame.
    window.setFoldSimulation('book')
    self:listen('windowFoldChanged', function(change)
        self.changes = self.changes + 1
        self:log('The fold changed to the posture "%s" with %d segments.', change.posture, #change.segments)
    end)
    self:frame{
        hint = 'The map and the instruments move to each side of the fold. Fold or turn a foldable device, or pick a simulated fold.',
        code = Folds.code,
        controls = {
            ui.sectionTitle{text = 'Simulate a fold'},
            ui.radioGroup{id = 'simulation', items = Folds.simulations, selected = 'book', onChange = function(event)
                window.setFoldSimulation(event.value ~= 'device' and event.value or nil)
            end},
        },
        focus = 'simulation',
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
    local count = #window.segments()
    local where = fold and string.format(' A %s fold, %s%s, crosses the window.', fold.axis, fold.state == 'flat' and 'opened flat' or 'half opened', fold.occluding and ' with a hinge' or '') or ' No fold crosses the window.'
    self:status(string.format('The device is %s, with %s.%s', Folds.postures[window.posture()] or window.posture(), count == 1 and 'one segment' or count .. ' segments', where))
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
        local size = math.min(whole.width, whole.height) * 0.5
        self:drawInstruments(m.rect(whole:right() - size * 0.86 - 24, whole:bottom() - size - 24, size * 0.86, size))
    end
    if crossing then
        self:drawFold(crossing, fold)
    end
end

-- The map fills its region with the painted island, cropped to the shape of the region, and a boat sails a loop around the island.
function Folds:drawMap(region)
    local texture = self.map
    local scale = math.max(region.width / texture.width, region.height / texture.height)
    local sourceWidth, sourceHeight = region.width / scale, region.height / scale
    graphics2d.draw(texture, region.x, region.y, {source = {(texture.width - sourceWidth) / 2, (texture.height - sourceHeight) / 2, sourceWidth, sourceHeight}, width = region.width, height = region.height, pivotX = 0, pivotY = 0})
    local x, y, heading = self:boatPlace(region)
    local size = math.max(48, math.min(region.width, region.height) * 0.12)
    graphics2d.drawShape({x + 4 - size * 0.22, y + 8 - size * 0.45, size * 0.44, size * 0.9}, {kind = 'ellipse', rotation = heading, color = '#40102030', softness = 8, layer = 2})
    graphics2d.draw(self.boat, x, y, {width = size, height = size, rotation = heading, layer = 3})
    graphics2d.drawShape({region.x + 16, region.y + 16, 230, 48}, {radius = 24, color = '#C0151826', layer = 4})
    Test.caption('Harbor map', region.x + 36, region.y + 40, {size = 26, color = Test.ink, anchor = {0, 0.5}, layer = 5})
end

-- Returns where the boat is on its loop around the island and the angle its bow points to, which turns it from pointing up.
function Folds:boatPlace(region)
    local angle = self.time * 0.35
    local x = region.x + region.width * (0.5 + 0.36 * math.cos(angle))
    local y = region.y + region.height * (0.5 + 0.36 * math.sin(angle))
    local dx, dy = -region.width * math.sin(angle), region.height * math.cos(angle)
    return x, y, math.atan(dy, dx) + math.pi / 2
end

-- A compass of the art style: a brass bezel with the four letters, a dark dial with ticks and a needle shaped as a diamond, red toward the heading of the boat, where north is up.
function Folds:drawInstruments(region)
    graphics2d.drawShape({region.x, region.y + 6, region.width, region.height}, {radius = 24, color = '#60000000', softness = 18, layer = 5})
    graphics2d.drawShape(region, {radius = 24, color = '#FF232938', borderWidth = 2, borderColor = Test.line, layer = 5})
    Test.caption('Compass', region.x + 24, region.y + 20, {size = 26, color = Test.ink, layer = 7})
    local radius = math.min(region.width, region.height - 120) * 0.38
    local x, y = region.x + region.width / 2, region.y + region.height / 2 + 6
    graphics2d.drawShape({x - radius - 14, y - radius - 14, (radius + 14) * 2, (radius + 14) * 2}, {kind = 'ellipse', color = '#FFE0A93C', borderWidth = 4, borderColor = Folds.outline, layer = 6})
    graphics2d.drawShape({x - radius, y - radius, radius * 2, radius * 2}, {kind = 'ellipse', color = '#FF1B2235', borderWidth = 3, borderColor = Folds.outline, layer = 7})
    for tick = 0, 23 do
        local angle = tick / 24 * math.pi * 2
        local inner = radius * (tick % 6 == 0 and 0.78 or 0.88)
        graphics2d.drawLine(x + math.cos(angle) * inner, y + math.sin(angle) * inner, x + math.cos(angle) * radius * 0.96, y + math.sin(angle) * radius * 0.96, tick % 6 == 0 and 3 or 2, '#99FFFFFF', {layer = 8})
    end
    for index, letter in ipairs({'N', 'E', 'S', 'W'}) do
        local angle = (index - 1) * math.pi / 2 - math.pi / 2
        Test.caption(letter, x + math.cos(angle) * radius * 0.6, y + math.sin(angle) * radius * 0.6, {size = math.max(18, radius * 0.22), color = letter == 'N' and Test.red or Test.ink, anchor = {0.5, 0.5}, layer = 8})
    end

    local _, _, heading = self:boatPlace(region)
    local long, wide = radius * 0.78, radius * 0.13
    local function point(along, across)
        return {x + math.sin(heading) * along + math.cos(heading) * across, y - math.cos(heading) * along + math.sin(heading) * across}
    end
    graphics2d.drawPolygon({point(long, 0), point(0, wide), point(0, -wide)}, '#FFF0473E', {layer = 9})
    graphics2d.drawPolygon({point(-long, 0), point(0, -wide), point(0, wide)}, '#FFE8EAF2', {layer = 9})
    graphics2d.drawPolyline({point(long, 0), point(0, wide), point(-long, 0), point(0, -wide)}, 3, Folds.outline, true, {layer = 10})
    graphics2d.drawShape({x - wide * 0.6, y - wide * 0.6, wide * 1.2, wide * 1.2}, {kind = 'ellipse', color = '#FFE0A93C', borderWidth = 2, borderColor = Folds.outline, layer = 11})

    local bearing = math.floor(math.deg(heading) % 360 + 0.5) % 360
    local direction = Folds.directions[math.floor((bearing + 22.5) / 45) % 8 + 1]
    Test.caption(string.format('Heading %d degrees, %s', bearing, direction), x, region.y + region.height - 24, {size = 24, color = Test.muted, anchor = {0.5, 1}, layer = 8})
end

-- A hinge hides what lies under it and draws as the dark spine between two screens, and a seamless fold is a soft crease the content keeps away from.
function Folds:drawFold(bounds, fold)
    if fold.occluding then
        graphics2d.drawShape(bounds, {radius = math.min(bounds.width, bounds.height) * 0.5, color = '#FF07080C', layer = 12})
        return
    end
    local center = bounds:center()
    local vertical = fold.axis == 'vertical'
    local crease = vertical and {center.x - 14, bounds.y, 28, bounds.height} or {bounds.x, center.y - 14, bounds.width, 28}
    graphics2d.drawShape(crease, {color = '#50000000', softness = 24, layer = 12})
    if vertical then
        graphics2d.drawLine(center.x, bounds.y, center.x, bounds:bottom(), 2, '#40FFFFFF', {layer = 13})
    else
        graphics2d.drawLine(bounds.x, center.y, bounds:right(), center.y, 2, '#40FFFFFF', {layer = 13})
    end
end

return Folds
