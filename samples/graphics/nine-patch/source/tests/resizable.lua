-- Resizable panel: a nine-slice window that the mouse or a finger moves by its middle and resizes by its edges and corners, while its text wraps inside the borders. WASD and the right stick resize it too.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local sample = require('sample')

local Resizable = haylen.class('Resizable', sample.Test)

local kBorder = 28
local kGrab = 22
local kFills = {{id = 'stretch', text = 'Stretch'}, {id = 'tile', text = 'Tile'}}
local kText = 'Drag the middle to move the panel and the edges or corners to resize it. The corners keep their size, the edges and the center follow, and this text wraps inside the borders.'
local kCode = [[
graphics2d.drawNineSlice(frame, panel, '#FFFFFFFF', nil, scale)  -- The variable `panel` is the rectangle the pointer edits.
graphics2d.pushClip(inner)  graphics2d.drawText(nil, text, inner.x, inner.y, {maxWidth = inner.width})  graphics2d.popClip()]]

function Resizable:enter()
    self.frameSlice = graphics2d.newNineSlice(sample.texture('frames/panel.png'), {borders = {kBorder, kBorder, kBorder, kBorder}})
    self.scale = 1
    self.panel = {x = 80, y = 60, width = 560, height = 340}
    self:frame({
        hint = 'Use the mouse or a finger on the stage, or WASD and the right stick.',
        code = kCode,
        controls = {
            ui.formField{label = 'Fill', ui.segmentedControl{id = 'fill', items = kFills, selected = 'stretch', onChange = function(event) self.frameSlice.fill = event.value end}},
            ui.formField{label = 'Border scale', ui.slider{id = 'scale', min = 0.5, max = 2, value = 1, step = 0.1, showValue = true, onChange = function(event) self.scale = event.value end}},
            ui.button{id = 'reset', text = 'Reset', onClick = function() self.panel = {x = 80, y = 60, width = 560, height = 340} end},
        },
        focus = 'fill',
    })
end

function Resizable:minimum()
    return kBorder * 2 * self.scale + 20
end

-- Finds what the point grabs: which edges it resizes, or the whole panel when it is inside.
function Resizable:grab(x, y)
    local panel = self.panel
    local right, bottom = panel.x + panel.width, panel.y + panel.height
    if x < panel.x - kGrab or x > right + kGrab or y < panel.y - kGrab or y > bottom + kGrab then
        return nil
    end
    local grab = {left = math.abs(x - panel.x) < kGrab, right = math.abs(x - right) < kGrab, top = math.abs(y - panel.y) < kGrab, bottom = math.abs(y - bottom) < kGrab}
    grab.move = not (grab.left or grab.right or grab.top or grab.bottom)
    return grab
end

-- Moves or resizes the panel from the rectangle it had when the drag started, keeping it in the stage and above its smallest size.
function Resizable:drag(x, y)
    local drag, start, minimum = self.dragging, self.dragging.start, self:minimum()
    local dx, dy = x - drag.x, y - drag.y
    local left, top, right, bottom = start.x, start.y, start.x + start.width, start.y + start.height
    if drag.grab.move then
        left, top, right, bottom = left + dx, top + dy, right + dx, bottom + dy
    end
    if drag.grab.left then
        left = math.min(left + dx, right - minimum)
    end
    if drag.grab.right then
        right = math.max(right + dx, left + minimum)
    end
    if drag.grab.top then
        top = math.min(top + dy, bottom - minimum)
    end
    if drag.grab.bottom then
        bottom = math.max(bottom + dy, top + minimum)
    end
    self.panel = {x = left, y = top, width = right - left, height = bottom - top}
end

function Resizable:update(dt)
    Resizable.super.update(self, dt)
    local x, y, pressed, held = self:pointer()
    if pressed then
        local grab = self:grab(x, y)
        if grab then
            self.dragging = {grab = grab, x = x, y = y, start = self.panel}
        end
    end
    if self.dragging and held then
        self:drag(x, y)
    elseif not held then
        self.dragging = nil
    end
    self.hover = self.dragging and self.dragging.grab or self:grab(x, y)

    local growX, growY = input.vector('resize')
    local minimum = self:minimum()
    self.panel.width = math.max(minimum, self.panel.width + growX * 400 * dt)
    self.panel.height = math.max(minimum, self.panel.height + growY * 400 * dt)
    self:status(string.format('panel %.0f, %.0f   %.0f x %.0f   %s', self.panel.x, self.panel.y, self.panel.width, self.panel.height, self.dragging and 'dragging' or 'released'))
end

function Resizable:draw(area)
    local panel = self.panel
    graphics2d.drawNineSlice(self.frameSlice, panel, '#FFFFFFFF', nil, self.scale)

    local inset = kBorder * self.scale
    local inner = {x = panel.x + inset, y = panel.y + inset, width = panel.width - inset * 2, height = panel.height - inset * 2}
    if inner.width > 0 and inner.height > 0 then
        graphics2d.pushClip(inner)
        graphics2d.drawText(nil, kText, inner.x + 10, inner.y + 8, {size = 26, color = '#FF3A2A1E', maxWidth = inner.width - 20, layer = 1})
        graphics2d.popClip()
    end

    -- Handles on the corners and the middle of every edge, lit while the pointer holds or points at them.
    local hover = self.hover
    local handles = {
        {panel.x, panel.y, 'left', 'top'}, {panel.x + panel.width, panel.y, 'right', 'top'},
        {panel.x, panel.y + panel.height, 'left', 'bottom'}, {panel.x + panel.width, panel.y + panel.height, 'right', 'bottom'},
        {panel.x + panel.width / 2, panel.y, 'top'}, {panel.x + panel.width / 2, panel.y + panel.height, 'bottom'},
        {panel.x, panel.y + panel.height / 2, 'left'}, {panel.x + panel.width, panel.y + panel.height / 2, 'right'},
    }
    for _, handle in ipairs(handles) do
        local lit = hover and hover[handle[3]] and (handle[4] == nil or hover[handle[4]])
        graphics2d.drawRect({handle[1] - 8, handle[2] - 8, 16, 16}, lit and sample.warm or '#C0FFFFFF', {layer = 2})
    end
end

return Resizable
