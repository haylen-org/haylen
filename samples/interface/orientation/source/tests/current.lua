-- Orientation and its event: window.orientation reads how the screen is turned, and windowOrientationChanged announces every turn, next to windowResized and windowSafeAreaChanged.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local sample = require('sample')

local Current = haylen.class('Current', sample.Test)

Current.hints = 'Turn a phone or a tablet, or resize a browser on one. Desktop windows, Mac Catalyst and TVs always count as landscape, so resizing a desktop window reports windowResized but never a turn.'

local kLogSize = 8

function Current:init(entry)
    Current.super.init(self, entry)
    self.log = {}
    self.phone = {angle = window.orientation() == 'portrait' and 0 or -math.pi / 2}
end

function Current:content()
    return ui.row{gap = 24,
        ui.column{width = 820, gap = 24, align = 'start',
            sample.section('now', {
                ui.label{id = 'orientation', text = window.orientation(), font = 'title', color = 'accentText'},
                ui.label{id = 'values', text = '', font = 'monospace'},
            }),
            sample.section('events', {ui.label{id = 'log', text = 'Turn the screen or resize the window.', font = 'monospace', color = 'textMuted'}}),
        },
        ui.spacer{id = 'stage', grow = 1, align = 'stretch'},
    }
end

function Current:record(line)
    table.insert(self.log, 1, string.format('%7.2f  %s', haylen.elapsed(), line))
    self.log[kLogSize + 1] = nil
    self.document:set('log', {text = table.concat(self.log, '\n')})
end

function Current:started()
    self:listen('windowOrientationChanged', function(event)
        self:record('windowOrientationChanged ' .. event.orientation)
        self.document:set('orientation', {text = event.orientation})
        tween.to(self.phone, 0.5, {angle = event.orientation == 'portrait' and 0 or -math.pi / 2}, {ease = 'backOut', owner = self, overwrite = true})
    end)
    self:listen('windowResized', function(event)
        self:record(string.format('windowResized %.0f x %.0f', event.width, event.height))
    end)
    self:listen('windowSafeAreaChanged', function(safe)
        self:record(string.format('windowSafeAreaChanged %.0f x %.0f', safe.width, safe.height))
    end)
end

function Current:update(dt)
    local width, height = window.framebufferSize()
    local visible = viewport.visibleRect()
    self:show('values', string.format('window  %.0f x %.0f pixels, dpi scale %g\nvisible %.0f x %.0f design units\nshape   %s', width, height, window.dpiScale(), visible.width, visible.height, sample.tall() and 'taller than wide' or 'wider than tall'))
    self:setStatus('window.orientation() = ' .. window.orientation())
end

-- A phone that turns with the screen, drawn in the free part of the page.
function Current:render()
    local stage = self.document:bounds('stage')
    if stage == nil then
        return
    end
    local cx, cy = stage.x + stage.width / 2, stage.y + stage.height / 2
    local size = math.min(stage.width, stage.height) * 0.4
    local angle = self.phone.angle
    local function point(x, y)
        return {cx + x * math.cos(angle) - y * math.sin(angle), cy + x * math.sin(angle) + y * math.cos(angle)}
    end
    local w, h = size * 0.5, size
    graphics2d.beginScreen()
    graphics2d.drawPolygon({point(-w, -h), point(w, -h), point(w, h), point(-w, h)}, '#FF2C3147')
    graphics2d.drawPolyline({point(-w, -h), point(w, -h), point(w, h), point(-w, h)}, 8, '#FF8FB0FF', true)
    graphics2d.drawPolygon({point(-w * 0.8, -h * 0.85), point(w * 0.8, -h * 0.85), point(w * 0.8, h * 0.8), point(-w * 0.8, h * 0.8)}, '#FF4C7DFF')
    local notch = point(0, -h * 0.92)
    graphics2d.drawCircle(notch[1], notch[2], size * 0.04, '#FF8FB0FF')
end

return Current
