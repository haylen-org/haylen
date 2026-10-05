-- The function `ui.setSafeAreaVisible` shades the screen outside the safe area, outlines it and prints its insets over everything, and `windowSafeAreaChanged` reports every move of the safe area.
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Test = require('harness.test')
local areas = require('categories.safe-area.areas')

local DebugOverlay = haylen.class('DebugOverlay', Test)

function DebugOverlay:init(entry)
    DebugOverlay.super.init(self, entry)
    self.changes = 0
    self.last = 'No change yet.'
end

function DebugOverlay:enter()
    self.simulation = areas.simulate()
    self.overlay = ui.safeAreaVisible()
    ui.setSafeAreaVisible(true)
    local simulation = viewport.safeAreaSimulation()
    self:frame{
        hint = 'The overlay is a debugging aid that "app.json" turns on at start with "debug.showSafeArea". Switch the device to watch the overlay and the event follow the new safe area.',
        focus = 'overlay',
        content = {ui.row{ui.spacer{grow = 1}, ui.card{width = 900, gap = 16,
            ui.toggle{id = 'overlay', text = 'Show the safe area overlay', checked = true, onChange = function(event)
                ui.setSafeAreaVisible(event.checked)
            end},
            ui.formField{label = 'Device', ui.combo{id = 'device', items = areas.devices, selected = type(simulation) == 'string' and simulation or 'device', onChange = function(event)
                viewport.setSafeAreaSimulation(event.value ~= 'device' and event.value or nil)
            end}},
            ui.label{id = 'rects', text = '', font = 'monospace'},
        }, ui.spacer{grow = 1}}},
    }
    self:listen('windowSafeAreaChanged', function(safe)
        self.changes = self.changes + 1
        self.last = string.format('The last one moved it to %.0f, %.0f, %.0f x %.0f.', safe.x, safe.y, safe.width, safe.height)
    end)
end

function DebugOverlay:exit()
    ui.setSafeAreaVisible(self.overlay)
    viewport.setSafeAreaSimulation(self.simulation)
    DebugOverlay.super.exit(self)
end

function DebugOverlay:update(dt)
    DebugOverlay.super.update(self, dt)
    local visible, safe = viewport.visibleRect(), viewport.safeRect()
    local top, right, bottom, left = areas.insets()
    local rects = string.format('Visible %.0f, %.0f, %.0f x %.0f\nSafe    %.0f, %.0f, %.0f x %.0f\nInsets  %.0f, %.0f, %.0f, %.0f', visible.x, visible.y, visible.width, visible.height, safe.x, safe.y, safe.width, safe.height, top, right, bottom, left)
    if rects ~= self.rects then
        self.rects = rects
        self:set('rects', {text = rects})
    end
    self:status(string.format('Overlay %s, %d safe area changes. %s', ui.safeAreaVisible() and 'shown' or 'hidden', self.changes, self.last))
end

return DebugOverlay
