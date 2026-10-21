-- The app draws its world over the whole screen, under the notch, the rounded corners and the home indicator, while a GUI keeps its HUD in the safe area. On a desktop the test simulates a phone with a dynamic island, so the frame of the test, which fills the safe area, leaves the edges of the screen to the world.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Test = require('harness.test')
local areas = require('categories.safe-area.areas')

local EdgeToEdge = haylen.class('EdgeToEdge', Test)

EdgeToEdge.code = [[
graphics2d.beginScreen()
graphics2d.draw(sea, visible.x, visible.y, {width = visible.width, height = visible.height, pivotX = 0, pivotY = 0})  -- Under the notch.
ui.mount(ui.stack{
    ui.panel{anchor = 'left', anchorTo = 'screen', margin = 12, ui.label{text = 'Under the notch'}},
    ui.row{anchor = 'stretchBottom', anchorTo = 'safe', margin = 24, ui.badge{text = '12 coins', tone = 'warning'}, ui.button{text = 'Map'}},
}, {placement = 'screen', owner = self})]]

-- The boats, each with the share of the sea it sails at, its size and its speed.
EdgeToEdge.boats = {{depth = 0.18, size = 120, speed = 0.018, start = 0.1}, {depth = 0.48, size = 190, speed = 0.026, start = 0.55}, {depth = 0.82, size = 260, speed = 0.034, start = 0.3}}

function EdgeToEdge:init(entry)
    EdgeToEdge.super.init(self, entry)
    self.edge = true
    self.border = true
end

-- The HUD is a GUI of its own under the frame, anchored to the safe area and kept inside the region of the test, and the label outside it sits against the screen, in the band of the notch of a phone held sideways.
function EdgeToEdge:enter()
    self.simulation = areas.simulate()
    self.sea = assets.texture('safe-area/sunset-sea.png', {filter = 'linear'})
    self.boat = assets.texture('safe-area/sailboat.png', {filter = 'linear'})
    self:frame{
        hint = 'Turn edge to edge off to box the world into the safe area, which leaves the edges of the screen empty. The HUD stays in the safe area either way, and one label sits against the screen on purpose.',
        code = EdgeToEdge.code,
        focus = 'edge',
        play = true,
        controls = {
            ui.sectionTitle{text = 'The world'},
            ui.toggle{id = 'edge', text = 'Draw the world edge to edge', checked = self.edge, onChange = function(event) self.edge = event.checked end},
            ui.toggle{id = 'border', text = 'Outline the safe area', checked = self.border, onChange = function(event) self.border = event.checked end},
        },
    }
    self.hud = ui.mount(ui.stack{
        ui.panel{anchor = 'left', anchorTo = 'screen', margin = 12, width = 112, padding = 10, ui.label{text = 'Under the notch', font = 'caption', textAlign = 'center', align = 'stretch'}},
        ui.row{id = 'hud', anchor = 'stretchBottom', anchorTo = 'safe', gap = 16,
            ui.circularProgress{value = 0.8, size = 72, tone = 'success', text = 'HP'},
            ui.panel{padding = {8, 16}, ui.label{text = 'Day 3', font = 'heading'}},
            ui.badge{text = '12 coins', tone = 'warning', solid = true},
            ui.spacer{grow = 1},
            ui.button{text = 'Map', focusable = false},
            ui.button{text = 'Bag', variant = 'primary', focusable = false},
        },
    }, {placement = 'screen', layer = -1, owner = self})
    self:keepInRegion(self.hud, 'hud', 24)
end

function EdgeToEdge:exit()
    viewport.setSafeAreaSimulation(self.simulation)
    EdgeToEdge.super.exit(self)
end

function EdgeToEdge:update(dt)
    EdgeToEdge.super.update(self, dt)
    local top, right, bottom, left = areas.insets()
    self:status(string.format('The safe area stays %.0f units from the top, %.0f from the right, %.0f from the bottom and %.0f from the left.', top, right, bottom, left))
end

-- A sea at sunset with boats that sail across it, drawn over the whole screen or only over the safe area, and a veil under the frame of the test, so its texts read over the scene while the region and the edges show it bright.
function EdgeToEdge:render()
    local visible, safe = viewport.visibleRect(), viewport.safeRect()
    local area = self.edge and visible or safe
    local time = haylen.elapsed()
    graphics2d.beginScreen()
    graphics2d.drawRect(visible, '#FF101418', {layer = -2})
    graphics2d.draw(self.sea, area.x, area.y, {width = area.width, height = area.height, pivotX = 0, pivotY = 0, layer = -1})
    local horizon = area.y + area.height * 0.545
    for index, boat in ipairs(EdgeToEdge.boats) do
        local x = area.x + (area.width + boat.size * 2) * ((time * boat.speed + boat.start) % 1) - boat.size
        local y = horizon + (area:bottom() - horizon) * boat.depth
        local bob = math.sin(time * 1.6 + index * 2) * boat.size * 0.02
        graphics2d.drawEllipse(x, y + boat.size * 0.02, boat.size * 0.42, boat.size * 0.05, '#55101830', {layer = index * 2})
        graphics2d.draw(self.boat, x, y + bob, {width = boat.size, height = boat.size, pivotY = 0.9, rotation = math.sin(time * 1.3 + index) * 0.04, layer = index * 2 + 1})
    end

    local region = self.gui and self.gui:bounds('region')
    if region then
        local veil = '#C0101418'
        graphics2d.drawRect({safe.x, safe.y, safe.width, region.y - safe.y}, veil, {layer = 20})
        graphics2d.drawRect({safe.x, region:bottom(), safe.width, safe:bottom() - region:bottom()}, veil, {layer = 20})
        graphics2d.drawRect({safe.x, region.y, region.x - safe.x, region.height}, veil, {layer = 20})
        graphics2d.drawRect({region:right(), region.y, safe:right() - region:right(), region.height}, veil, {layer = 20})
    end
    if self.border then
        graphics2d.drawShape({safe.x, safe.y, safe.width, safe.height}, {radius = 16, color = '#00000000', borderWidth = 3, borderColor = '#FF3AA8E0', layer = 21})
    end
end

return EdgeToEdge
