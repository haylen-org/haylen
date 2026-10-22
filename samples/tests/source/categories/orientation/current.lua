-- The function `window.orientation` reads how the screen is turned, and `windowOrientationChanged` announces every turn, next to `windowResized` and `windowSafeAreaChanged`. The project starts locked to landscape, so the test lets the screen turn while it shows.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Device = require('categories.safe-area.device')
local Test = require('harness.test')
local areas = require('categories.safe-area.areas')

local Current = haylen.class('Current', Test)

Current.code = [[
window.lockOrientation('any')
print(window.orientation())  -- Prints "portrait" or "landscape".
events.on('windowOrientationChanged', function(event) layOut(event.orientation) end, {owner = self})
events.on('windowResized', function(event) print(event.width, event.height) end, {owner = self})]]

Current.lineCount = 8
-- The width of the phone held upright, as a share of its height.
Current.phoneShape = 0.49

local function tall()
    local visible = viewport.visibleRect()
    return visible.height > visible.width
end

function Current:init(entry)
    Current.super.init(self, entry)
    self.lines = {}
    self.phone = {angle = window.orientation() == 'portrait' and 0 or -math.pi / 2}
end

function Current:enter()
    window.lockOrientation('any')
    self.wallpaper = areas.wallpaper()
    self:frame{
        hint = 'Turn a phone or a tablet, or resize a browser on one. Desktop windows, Mac Catalyst and TVs always count as landscape, so resizing a desktop window reports "windowResized" but never a turn.',
        code = Current.code,
        panelWidth = 760,
        controls = {
            ui.sectionTitle{text = 'Now'},
            ui.label{id = 'orientation', text = 'Orientation "' .. window.orientation() .. '"', font = 'title', color = 'accentText'},
            ui.table{id = 'values', columns = {{text = 'Measure', width = 200}, {text = 'Value'}}},
            ui.sectionTitle{text = 'Events'},
            ui.label{id = 'events', text = 'Turn the screen or resize the window.', color = 'textMuted'},
        },
    }
    self:listen('windowOrientationChanged', function(event)
        self:record('The screen turned to ' .. event.orientation)
        self:set('orientation', {text = 'Orientation "' .. event.orientation .. '"'})
        tween.to(self.phone, 0.5, {angle = event.orientation == 'portrait' and 0 or -math.pi / 2}, {ease = 'backOut', owner = self, overwrite = true})
    end)
    self:listen('windowResized', function(event)
        self:record(string.format('The window became %.0f by %.0f', event.width, event.height))
    end)
    self:listen('windowSafeAreaChanged', function(safe)
        self:record(string.format('The safe area became %.0f by %.0f', safe.width, safe.height))
    end)
end

function Current:exit()
    window.lockOrientation('landscape')
    Current.super.exit(self)
end

function Current:record(line)
    table.insert(self.lines, 1, string.format('%s at %.1f seconds.', line, haylen.elapsed()))
    self.lines[Current.lineCount + 1] = nil
    self:set('events', {text = table.concat(self.lines, '\n')})
end

function Current:update(dt)
    Current.super.update(self, dt)
    local width, height = window.framebufferSize()
    local visible = viewport.visibleRect()
    local values = string.format('%.0f by %.0f pixels|%g pixels for each point|%.0f by %.0f design units|%s', width, height, window.dpiScale(), visible.width, visible.height, tall() and 'Taller than wide' or 'Wider than tall')
    if values ~= self.values then
        self.values = values
        local cells = {}
        for value in values:gmatch('[^|]+') do
            cells[#cells + 1] = value
        end
        self:set('values', {rows = {
            {id = 'window', cells = {'Window', cells[1]}},
            {id = 'density', cells = {'Density', cells[2]}},
            {id = 'visible', cells = {'Visible', cells[3]}},
            {id = 'shape', cells = {'Shape', cells[4]}},
        }})
    end
    self:status('The call "window.orientation()" returns "' .. window.orientation() .. '".')
end

-- A phone that turns with the screen and keeps its wallpaper upright, over a soft glow in the middle of the play area.
function Current:draw(area)
    local height = math.min(area.height * 0.72, area.width * 0.6)
    local glow = height * 0.66
    graphics2d.drawShape({area.width / 2 - glow, area.height / 2 - glow, glow * 2, glow * 2}, {kind = 'ellipse', color = '#1AFFFFFF', softness = glow * 0.5, layer = -5})
    Device.draw{kind = 'island', x = area.width / 2, y = area.height / 2, width = height * Current.phoneShape, height = height, angle = self.phone.angle, wallpaper = self.wallpaper}
end

return Current
