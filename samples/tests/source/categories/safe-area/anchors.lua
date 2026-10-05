-- A node with an anchor leaves the layout of its parent and sits against the safe area or the whole screen, at one of 16 presets and a margin away from the edges.
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Test = require('harness.test')
local areas = require('categories.safe-area.areas')

local Anchors = haylen.class('Anchors', Test)

Anchors.points = {'topLeft', 'top', 'topRight', 'left', 'center', 'right', 'bottomLeft', 'bottom', 'bottomRight'}
Anchors.stretches = {'stretch', 'stretchTop', 'stretchBottom', 'stretchLeft', 'stretchRight', 'stretchHorizontal', 'stretchVertical'}

-- The size along the axis a stretch preset leaves free.
Anchors.sizes = {stretchTop = {height = 120}, stretchBottom = {height = 120}, stretchHorizontal = {height = 120}, stretchLeft = {width = 300}, stretchRight = {width = 300}, stretchVertical = {width = 300}}

function Anchors:init(entry)
    Anchors.super.init(self, entry)
    self.anchor = 'points'
    self.areaName = 'both'
    self.margin = 24
end

-- The anchored nodes live in a GUI of the whole screen under the frame, so every preset shows and the frame stays usable.
function Anchors:enter()
    self.simulation = areas.simulate()
    self:frame{
        hint = 'Pick a preset with the stepper, or with left and right while it has the focus. Blue nodes anchor to the safe area and orange ones to the whole screen, and the red bands mark what lies outside the safe area.',
        focus = 'anchor',
        content = {ui.row{ui.spacer{grow = 1}, ui.card{width = 900, gap = 16, children = self:controls()}, ui.spacer{grow = 1}}},
    }
    self.demo = ui.mount(ui.stack{id = 'demo', children = self:nodes()}, {placement = 'screen', layer = -1, owner = self})
    self:refresh()
end

function Anchors:exit()
    viewport.setSafeAreaSimulation(self.simulation)
    Anchors.super.exit(self)
end

function Anchors:node(anchor, area)
    local node = ui.alert{anchor = anchor, anchorTo = area, margin = self.margin, tone = area == 'safe' and 'information' or 'warning', title = 'Anchor "' .. anchor .. '"', message = area == 'safe' and 'Safe area' or 'Screen'}
    for key, value in pairs(Anchors.sizes[anchor] or {}) do
        node[key] = value
    end
    return node
end

function Anchors:nodes()
    local anchors = self.anchor == 'points' and Anchors.points or {self.anchor}
    local names = self.areaName == 'both' and {'screen', 'safe'} or {self.areaName}
    local nodes = {}
    for _, area in ipairs(names) do
        for _, anchor in ipairs(anchors) do
            nodes[#nodes + 1] = self:node(anchor, area)
        end
    end
    return nodes
end

function Anchors:refresh()
    self.demo:replaceChildren('demo', self:nodes())
    local preset = self.anchor == 'points' and 'The nine points' or 'Anchor "' .. self.anchor .. '"'
    local against = self.areaName == 'both' and 'the safe area and the screen' or 'the ' .. (self.areaName == 'safe' and 'safe area' or 'screen')
    self:set('status', {text = string.format('%s against %s, margin %d.', preset, against, self.margin)})
end

function Anchors:controls()
    local presets = {{id = 'points', text = 'The nine points'}}
    for _, name in ipairs(Anchors.points) do
        presets[#presets + 1] = {id = name, text = 'Anchor "' .. name .. '"'}
    end
    for _, name in ipairs(Anchors.stretches) do
        presets[#presets + 1] = {id = name, text = 'Anchor "' .. name .. '"'}
    end
    return {
        ui.settingsRow{label = 'Preset', caption = 'Nine points and seven stretches', ui.stepper{id = 'anchor', width = 440, items = presets, selected = self.anchor, wrap = true, onChange = function(event)
            self.anchor = event.value
            self:refresh()
        end}},
        ui.settingsRow{label = 'Area', ui.segmentedControl{id = 'area', width = 440, items = {{id = 'safe', text = 'Safe'}, {id = 'screen', text = 'Screen'}, {id = 'both', text = 'Both'}}, selected = self.areaName, onChange = function(event)
            self.areaName = event.value
            self:refresh()
        end}},
        ui.settingsRow{label = 'Margin', ui.stepper{id = 'margin', width = 440, value = self.margin, min = 0, max = 96, step = 24, onChange = function(event)
            self.margin = event.value
            self:refresh()
        end}},
    }
end

function Anchors:render()
    areas.drawBands()
end

return Anchors
