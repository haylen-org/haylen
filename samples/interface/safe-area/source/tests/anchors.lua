-- Anchors: a node with an anchor leaves the layout of its parent and sits against the safe area or the whole screen, at one of 16 presets and a margin away from the edges.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local sample = require('sample')

local Anchors = haylen.class('Anchors', sample.Test)

Anchors.hints = 'Pick a preset with the stepper, or with left and right while it has the focus. Blue nodes anchor to the safe area and orange ones to the whole screen, and the red bands mark what lies outside the safe area.'
Anchors.focus = 'anchor'

local kPoints = {'topLeft', 'top', 'topRight', 'left', 'center', 'right', 'bottomLeft', 'bottom', 'bottomRight'}
local kStretches = {'stretch', 'stretchTop', 'stretchBottom', 'stretchLeft', 'stretchRight', 'stretchHorizontal', 'stretchVertical'}

-- The size along the axis a stretch preset leaves free.
local kSizes = {stretchTop = {height = 120}, stretchBottom = {height = 120}, stretchHorizontal = {height = 120}, stretchLeft = {width = 300}, stretchRight = {width = 300}, stretchVertical = {width = 300}}

function Anchors:init(entry)
    Anchors.super.init(self, entry)
    self.anchor = 'points'
    self.area = 'both'
    self.margin = 24
end

function Anchors:node(anchor, area)
    local node = ui.alert{anchor = anchor, anchorTo = area, margin = self.margin, tone = area == 'safe' and 'information' or 'warning', title = anchor, message = area == 'safe' and 'safe area' or 'screen'}
    for key, value in pairs(kSizes[anchor] or {}) do
        node[key] = value
    end
    return node
end

function Anchors:nodes()
    local anchors = self.anchor == 'points' and kPoints or {self.anchor}
    local areas = self.area == 'both' and {'screen', 'safe'} or {self.area}
    local nodes = {}
    for _, area in ipairs(areas) do
        for _, anchor in ipairs(anchors) do
            nodes[#nodes + 1] = self:node(anchor, area)
        end
    end
    return nodes
end

function Anchors:refresh()
    self.demo:replace('demo', self:nodes())
    self:setStatus(string.format('%s, %s, margin %d', self.anchor == 'points' and 'the nine points' or self.anchor, self.area == 'both' and 'safe area and screen' or self.area, self.margin))
end

function Anchors:controls()
    local presets = {{id = 'points', text = 'The nine points'}}
    for _, name in ipairs(kPoints) do
        presets[#presets + 1] = {id = name, text = name}
    end
    for _, name in ipairs(kStretches) do
        presets[#presets + 1] = {id = name, text = name}
    end
    return {
        ui.settingsRow{label = 'Preset', caption = '9 points and 7 stretches', ui.stepper{id = 'anchor', width = 440, items = presets, selected = self.anchor, wrap = true, onChange = function(event)
            self.anchor = event.value
            self:refresh()
        end}},
        ui.settingsRow{label = 'Area', ui.segmentedControl{id = 'area', width = 440, items = {{id = 'safe', text = 'Safe'}, {id = 'screen', text = 'Screen'}, {id = 'both', text = 'Both'}}, selected = self.area, onChange = function(event)
            self.area = event.value
            self:refresh()
        end}},
        ui.settingsRow{label = 'Margin', ui.stepper{id = 'margin', width = 440, value = self.margin, min = 0, max = 96, step = 24, onChange = function(event)
            self.margin = event.value
            self:refresh()
        end}},
    }
end

-- The anchored nodes live in a document of the whole screen above the frame, so every preset shows even where the frame is.
function Anchors:started()
    self.demo = sample.mount(self, ui.stack{id = 'demo', children = self:nodes()}, {placement = 'screen', layer = 2})
    self:refresh()
end

function Anchors:render()
    graphics2d.beginScreen()
    sample.drawBands('#40FF4040')
    graphics2d.drawRectOutline(viewport.safeRect(), 3, '#FF3AA8E0')
end

return Anchors
