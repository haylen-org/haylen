-- The function `window.orientation` reads how the screen is turned, and `windowOrientationChanged` announces every turn, next to `windowResized` and `windowSafeAreaChanged`. The project starts locked to landscape, so the test lets the screen turn while it shows.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Test = require('harness.test')

local Current = haylen.class('Current', Test)

Current.lineCount = 8

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
    self:frame{
        hint = 'Turn a phone or a tablet, or resize a browser on one. Desktop windows, Mac Catalyst and TVs always count as landscape, so resizing a desktop window reports "windowResized" but never a turn.',
        panelWidth = 760,
        controls = {
            ui.sectionTitle{text = 'Now'},
            ui.label{id = 'orientation', text = 'Orientation "' .. window.orientation() .. '"', font = 'title', color = 'accentText'},
            ui.label{id = 'values', text = '', font = 'monospace'},
            ui.sectionTitle{text = 'Events'},
            ui.label{id = 'events', text = 'Turn the screen or resize the window.', font = 'monospace', color = 'textMuted'},
        },
    }
    self:listen('windowOrientationChanged', function(event)
        self:record('Event "windowOrientationChanged" to "' .. event.orientation .. '"')
        self:set('orientation', {text = 'Orientation "' .. event.orientation .. '"'})
        tween.to(self.phone, 0.5, {angle = event.orientation == 'portrait' and 0 or -math.pi / 2}, {ease = 'backOut', owner = self, overwrite = true})
    end)
    self:listen('windowResized', function(event)
        self:record(string.format('Event "windowResized" %.0f x %.0f', event.width, event.height))
    end)
    self:listen('windowSafeAreaChanged', function(safe)
        self:record(string.format('Event "windowSafeAreaChanged" %.0f x %.0f', safe.width, safe.height))
    end)
end

function Current:exit()
    window.lockOrientation('landscape')
    Current.super.exit(self)
end

function Current:record(line)
    table.insert(self.lines, 1, string.format('%7.2f  %s', haylen.elapsed(), line))
    self.lines[Current.lineCount + 1] = nil
    self:set('events', {text = table.concat(self.lines, '\n')})
end

function Current:update(dt)
    Current.super.update(self, dt)
    local width, height = window.framebufferSize()
    local visible = viewport.visibleRect()
    local values = string.format('Window  %.0f x %.0f pixels, DPI scale %g\nVisible %.0f x %.0f design units\nShape   %s', width, height, window.dpiScale(), visible.width, visible.height, tall() and 'Taller than wide' or 'Wider than tall')
    if values ~= self.values then
        self.values = values
        self:set('values', {text = values})
    end
    self:status('The call "window.orientation()" returns "' .. window.orientation() .. '".')
end

-- A phone that turns with the screen.
function Current:draw(area)
    local cx, cy = area.width / 2, area.height / 2
    local size = math.min(area.width, area.height) * 0.4
    local angle = self.phone.angle
    local function point(x, y)
        return {cx + x * math.cos(angle) - y * math.sin(angle), cy + x * math.sin(angle) + y * math.cos(angle)}
    end
    local w, h = size * 0.5, size
    graphics2d.drawPolygon({point(-w, -h), point(w, -h), point(w, h), point(-w, h)}, Test.line)
    graphics2d.drawPolyline({point(-w, -h), point(w, -h), point(w, h), point(-w, h)}, 8, Test.accent, true)
    graphics2d.drawPolygon({point(-w * 0.8, -h * 0.85), point(w * 0.8, -h * 0.85), point(w * 0.8, h * 0.8), point(-w * 0.8, h * 0.8)}, '#FF4C7DFF')
    local notch = point(0, -h * 0.92)
    graphics2d.drawCircle(notch[1], notch[2], size * 0.04, Test.accent)
end

return Current
