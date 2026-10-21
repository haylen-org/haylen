-- A node with an anchor leaves the layout of its parent and sits against an area at one of 16 presets and a margin away from its edges: the safe area, the whole screen or its parent. The test shows them on a phone held sideways inside the region of the test, whose screen and safe area are two stacks that the markers anchor to as their parents, as an app anchors to the screen and the safe area of the device.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Device = require('categories.safe-area.device')
local Test = require('harness.test')

local Anchors = haylen.class('Anchors', Test)

Anchors.code = [[
ui.badge{text = 'topRight', anchor = 'topRight', anchorTo = 'safe', margin = 24}  -- Against the safe area of the device.
ui.panel{anchor = 'stretchBottom', anchorTo = 'screen', height = 80}  -- Reaches the edges of the screen.
ui.stack{id = 'screen', aspectRatio = 2532 / 1170, align = 'stretch',  -- The phone of the test.
    ui.stack{id = 'safe', align = 'stretch', margin = insets, ui.badge{text = 'top', anchor = 'top', anchorTo = 'parent', margin = 24}},
}]]

Anchors.points = {'topLeft', 'top', 'topRight', 'left', 'center', 'right', 'bottomLeft', 'bottom', 'bottomRight'}
Anchors.stretches = {'stretch', 'stretchTop', 'stretchBottom', 'stretchLeft', 'stretchRight', 'stretchHorizontal', 'stretchVertical'}
-- The size along the axis a stretch preset leaves free, as a share of the screen.
Anchors.sizes = {stretchTop = {height = 0.16}, stretchBottom = {height = 0.16}, stretchHorizontal = {height = 0.16}, stretchLeft = {width = 0.16}, stretchRight = {width = 0.16}, stretchVertical = {width = 0.16}}
-- The screen of a phone with a dynamic island held sideways, and its safe area as shares of the screen: the island on the left, the corners on the right and the home indicator at the bottom.
Anchors.screen = {2532, 1170}
Anchors.insets = {0, 0.069, 0.054, 0.069}

function Anchors:init(entry)
    Anchors.super.init(self, entry)
    self.anchor = 'points'
    self.areaName = 'safe'
    self.margin = 24
end

function Anchors:enter()
    self:frame{
        hint = 'Pick a preset with the stepper, or with left and right while it has the focus. Blue markers anchor to the safe area and orange ones to the whole screen, and the shaded bands of the phone lie outside its safe area.',
        code = Anchors.code,
        focus = 'anchor',
        fixed = true,
        content = {ui.row{grow = 1, gap = 24, align = 'stretch',
            ui.stack{id = 'room', grow = 1, align = 'stretch',
                ui.stack{id = 'screen', aspectRatio = Anchors.screen[1] / Anchors.screen[2], align = 'stretch', margin = {56, 72},
                    ui.stack{id = 'safe', align = 'stretch'},
                },
            },
            ui.card{width = 560, gap = 16, align = 'start', children = self:controls()},
        }},
    }
    self:refresh()
end

-- A marker is a small badge in the color of its area, or a translucent band with its badge for a stretch.
function Anchors:node(anchor, area, size)
    local tone = area == 'safe' and 'information' or 'warning'
    local badge = ui.badge{text = anchor, tone = tone, solid = true}
    local stretch = Anchors.sizes[anchor] or (anchor == 'stretch' and {}) or nil
    if not stretch then
        badge.anchor, badge.anchorTo, badge.margin = anchor, 'parent', self.margin
        return badge
    end
    badge.align = 'center'
    local band = ui.stack{anchor = anchor, anchorTo = 'parent', margin = self.margin, style = {colors = {panel = area == 'safe' and '#663AA8E0' or '#66F2B23A'}}, ui.panel{align = 'stretch', padding = 0}, badge}
    for key, share in pairs(stretch) do
        band[key] = math.floor((key == 'width' and size.width or size.height) * share)
    end
    return band
end

function Anchors:refresh()
    local size = self.gui:bounds('screen') or {width = 1200, height = 560}
    local anchors = self.anchor == 'points' and Anchors.points or {self.anchor}
    local screen, safe = {ui.stack{id = 'safe', align = 'stretch', margin = self:safeMargins(size)}}, {}
    for _, anchor in ipairs(anchors) do
        if self.areaName ~= 'safe' then
            screen[#screen + 1] = self:node(anchor, 'screen', size)
        end
        if self.areaName ~= 'screen' then
            safe[#safe + 1] = self:node(anchor, 'safe', size)
        end
    end
    self.gui:replaceChildren('screen', screen)
    self.gui:replaceChildren('safe', safe)
    self.size = {size.width, size.height}
    local preset = self.anchor == 'points' and 'The nine points' or 'The anchor "' .. self.anchor .. '"'
    local against = self.areaName == 'both' and 'the safe area and the screen' or 'the ' .. (self.areaName == 'safe' and 'safe area' or 'screen')
    self:set('status', {text = string.format('%s against %s, %d units from their edges.', preset, against, self.margin)})
end

-- The margins of the safe area inside the screen, the insets of the phone for the size the screen has.
function Anchors:safeMargins(size)
    local insets = Anchors.insets
    return {math.floor(size.height * insets[1]), math.floor(size.width * insets[2]), math.floor(size.height * insets[3]), math.floor(size.width * insets[4])}
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
        ui.sectionTitle{text = 'Anchors'},
        ui.formField{label = 'Preset', help = 'Nine points and seven stretches', ui.stepper{id = 'anchor', items = presets, selected = self.anchor, wrap = true, onChange = function(event)
            self.anchor = event.value
            self:refresh()
        end}},
        ui.formField{label = 'Area', ui.segmentedControl{id = 'area', items = {{id = 'safe', text = 'Safe area'}, {id = 'screen', text = 'Screen'}, {id = 'both', text = 'Both'}}, selected = self.areaName, onChange = function(event)
            self.areaName = event.value
            self:refresh()
        end}},
        ui.formField{label = 'Margin', ui.stepper{id = 'margin', value = self.margin, min = 0, max = 96, step = 24, onChange = function(event)
            self.margin = event.value
            self:refresh()
        end}},
    }
end

-- The anchors follow the size of the screen, which changes with the window.
function Anchors:update(dt)
    Anchors.super.update(self, dt)
    local size = self.gui:bounds('screen')
    if size and (not self.size or self.size[1] ~= size.width or self.size[2] ~= size.height) then
        self:refresh()
    end
end

-- The phone holds the screen stack in its screen, with the bands outside the safe area shaded and the safe area outlined.
function Anchors:render()
    local screen = self.gui and self.gui:bounds('screen')
    local safe = self.gui and self.gui:bounds('safe')
    if not screen or not safe then
        return
    end
    graphics2d.beginScreen()
    local device, radius = Device.around('island', {screen.x, screen.y, screen.width, screen.height}, true)
    device.screen = '#FF1E3A5F'
    Device.draw(device)
    graphics2d.pushMask(function()
        graphics2d.drawShape({screen.x, screen.y, screen.width, screen.height}, {radius = radius, color = '#FFFFFFFF'})
    end, {layer = 3})
    graphics2d.drawRect({screen.x, screen.y, screen.width, screen.height}, '#26F0473E')
    graphics2d.drawRect({safe.x, safe.y, safe.width, safe.height}, '#FF1E3A5F', {layer = 1})
    graphics2d.popMask()
    graphics2d.drawShape({safe.x, safe.y, safe.width, safe.height}, {radius = 12, color = '#00000000', borderWidth = 3, borderColor = '#B33AA8E0', layer = 5})
end

return Anchors
