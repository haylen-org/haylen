-- Debug overlay: ui.setSafeAreaVisible shades the screen outside the safe area, outlines it and prints its insets over everything, and windowSafeAreaChanged reports every move of the safe area.
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local sample = require('sample')

local DebugOverlay = haylen.class('DebugOverlay', sample.Test)

DebugOverlay.hints = 'The overlay is a debugging aid that app.json turns on at start with debug.showSafeArea. Switch the device to watch the overlay and the event follow the new safe area.'
DebugOverlay.focus = 'overlay'

function DebugOverlay:init(entry)
    DebugOverlay.super.init(self, entry)
    self.changes = 0
    self.last = 'no change yet'
end

function DebugOverlay:controls()
    local simulation = viewport.safeAreaSimulation()
    return {
        ui.toggle{id = 'overlay', text = 'Show the safe area overlay', checked = true, onChange = function(event)
            ui.setSafeAreaVisible(event.checked)
        end},
        ui.settingsRow{label = 'Device', ui.combo{id = 'device', width = 480, items = sample.devices, selected = type(simulation) == 'string' and simulation or 'device', onChange = function(event)
            viewport.setSafeAreaSimulation(event.value ~= 'device' and event.value or nil)
        end}},
        ui.label{id = 'rects', text = '', font = 'monospace'},
    }
end

function DebugOverlay:started()
    ui.setSafeAreaVisible(true)
    self:listen('windowSafeAreaChanged', function(safe)
        self.changes = self.changes + 1
        self.last = string.format('changed to %.0f, %.0f, %.0f x %.0f', safe.x, safe.y, safe.width, safe.height)
    end)
end

function DebugOverlay:exit()
    ui.setSafeAreaVisible(false)
end

function DebugOverlay:update(dt)
    local visible, safe = viewport.visibleRect(), viewport.safeRect()
    local top, right, bottom, left = sample.insets()
    local rects = string.format('visible %.0f, %.0f, %.0f x %.0f\nsafe    %.0f, %.0f, %.0f x %.0f\ninsets  %.0f, %.0f, %.0f, %.0f', visible.x, visible.y, visible.width, visible.height, safe.x, safe.y, safe.width, safe.height, top, right, bottom, left)
    if rects ~= self.rects then
        self.rects = rects
        self.frame:set('rects', {text = rects})
    end
    self:setStatus(string.format('overlay %s, %d safe area changes, %s', ui.safeAreaVisible() and 'shown' or 'hidden', self.changes, self.last))
end

return DebugOverlay
