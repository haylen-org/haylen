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
graphics2d.draw(sea, visible.x, visible.y, {width = visible.width, height = visible.height, source = crop, pivotX = 0, pivotY = 0})  -- Under the notch.
ui.mount(ui.stack{
    ui.panel{anchor = 'left', anchorTo = 'screen', margin = 12, ui.label{text = 'Under the notch'}},
    ui.row{anchor = 'stretchTop', anchorTo = 'safe', margin = 24, ui.badge{text = '12 coins', tone = 'warning'}, ui.button{text = 'Map'}},
}, {placement = 'screen', owner = self})]]

-- The share of the height of the sea picture where its horizon lies.
EdgeToEdge.horizon = 0.545

-- The boats, each with the share of the sea in view it sails at, its size as a share of the height of the region and its speed.
EdgeToEdge.boats = {{depth = 0.14, size = 0.17, speed = 0.022, start = 0.15}, {depth = 0.5, size = 0.26, speed = 0.03, start = 0.6}, {depth = 0.86, size = 0.36, speed = 0.038, start = 0.35}}

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
            ui.toggle{id = 'edge', text = 'Draw edge to edge', checked = self.edge, onChange = function(event) self.edge = event.checked end},
            ui.toggle{id = 'border', text = 'Outline the safe area', checked = self.border, onChange = function(event) self.border = event.checked end},
        },
    }
    self.hud = ui.mount(ui.stack{
        ui.panel{anchor = 'left', anchorTo = 'screen', margin = 12, width = 112, padding = 10, ui.label{text = 'Under the notch', font = 'caption', textAlign = 'center', align = 'stretch'}},
        ui.row{id = 'hud', anchor = 'stretchTop', anchorTo = 'safe', gap = 16,
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

-- A sea at sunset over the whole screen or only over the safe area, placed so that its horizon crosses the middle of the region of the test, with boats that sail across the region. The frame of the test covers the rest of the safe area, so its texts never lie over the scene, while the region and the edges of the screen show it.
function EdgeToEdge:render()
    local region = self.gui and self.gui:bounds('region')
    if not region then
        return
    end
    local visible, safe = viewport.visibleRect(), viewport.safeRect()
    local area = self.edge and visible or safe
    local horizon = region.y + region.height * 0.5
    graphics2d.beginScreen()
    graphics2d.drawRect(visible, haylen.config.clearColor, {layer = -2})
    graphics2d.draw(self.sea, area.x, area.y, {width = area.width, height = area.height, source = self:seaCrop(area, horizon), pivotX = 0, pivotY = 0, layer = -1})
    self:drawBoats(region, horizon)

    local frame = haylen.config.clearColor
    graphics2d.drawRect({safe.x, safe.y, safe.width, region.y - safe.y}, frame, {layer = 20})
    graphics2d.drawRect({safe.x, region:bottom(), safe.width, safe:bottom() - region:bottom()}, frame, {layer = 20})
    graphics2d.drawRect({safe.x, region.y, region.x - safe.x, region.height}, frame, {layer = 20})
    graphics2d.drawRect({region:right(), region.y, safe:right() - region:right(), region.height}, frame, {layer = 20})
    if self.border then
        graphics2d.drawShape({safe.x, safe.y, safe.width, safe.height}, {radius = 16, color = '#00000000', borderWidth = 3, borderColor = '#FF3AA8E0', layer = 21})
    end
end

-- The part of the sea picture that covers `area` with the horizon of the picture at the height `horizon`, keeping the proportions of the picture.
function EdgeToEdge:seaCrop(area, horizon)
    local width, height = self.sea.width, self.sea.height
    local share = EdgeToEdge.horizon
    local scale = math.max(area.width / width, area.height / height, (horizon - area.y) / (height * share), (area:bottom() - horizon) / (height * (1 - share)))
    local left = area.x + (area.width - width * scale) / 2
    local top = horizon - height * share * scale
    return {(area.x - left) / scale, (area.y - top) / scale, area.width / scale, area.height / scale}
end

-- The boats sail between the horizon and the bottom of the region, smaller the farther they are, inside the rounded region.
function EdgeToEdge:drawBoats(region, horizon)
    local time = haylen.elapsed()
    graphics2d.pushMask(function()
        graphics2d.drawShape({region.x, region.y, region.width, region.height}, {radius = ui.themeMetric('panelRadius'), color = '#FFFFFFFF'})
    end, {layer = 1})
    for index, boat in ipairs(EdgeToEdge.boats) do
        local size = region.height * boat.size
        local x = region.x + (region.width + size * 2) * ((time * boat.speed + boat.start) % 1) - size
        local y = horizon + (region:bottom() - horizon - size * 0.14) * boat.depth
        local bob = math.sin(time * 1.6 + index * 2) * size * 0.02
        graphics2d.drawEllipse(x, y + size * 0.02, size * 0.34, size * 0.035, '#55101830', {layer = index * 2})
        graphics2d.draw(self.boat, x, y + bob, {width = size, height = size, pivotY = 0.9, rotation = math.sin(time * 1.3 + index) * 0.04, layer = index * 2 + 1})
    end
    graphics2d.popMask()
end

return EdgeToEdge
