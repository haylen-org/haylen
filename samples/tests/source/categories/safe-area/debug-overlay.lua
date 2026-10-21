-- The function `ui.setSafeAreaVisible` shades the screen outside the safe area, outlines it and prints its insets over everything, and `windowSafeAreaChanged` reports every move of the safe area.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Test = require('harness.test')
local areas = require('categories.safe-area.areas')

local DebugOverlay = haylen.class('DebugOverlay', Test)

DebugOverlay.code = [[
ui.setSafeAreaVisible(true)  -- Or `"debug": {"showSafeArea": true}` in `app.json`.
events.on('windowSafeAreaChanged', function(safe) print(safe.x, safe.y, safe.width, safe.height) end, {owner = self})
local visible, safe = viewport.visibleRect(), viewport.safeRect()]]

function DebugOverlay:init(entry)
    DebugOverlay.super.init(self, entry)
    self.changes = 0
    self.device = 'device'
end

function DebugOverlay:enter()
    self.simulation = areas.simulate()
    self.overlay = ui.safeAreaVisible()
    ui.setSafeAreaVisible(true)
    local simulation = viewport.safeAreaSimulation()
    self.device = type(simulation) == 'string' and simulation or 'device'
    self:frame{
        hint = 'The overlay is a debugging aid that "app.json" turns on at start with "debug.showSafeArea". Switch the device to watch the overlay and the event follow the new safe area.',
        code = DebugOverlay.code,
        focus = 'overlay',
        fixed = true,
        content = {ui.row{grow = 1, gap = 24, align = 'stretch',
            ui.card{width = 760, gap = 16,
                ui.toggle{id = 'overlay', text = 'Show the safe area overlay', checked = true, onChange = function(event)
                    ui.setSafeAreaVisible(event.checked)
                end},
                ui.formField{label = 'Device', ui.combo{id = 'device', items = areas.devices, selected = self.device, onChange = function(event)
                    self.device = event.value
                    viewport.setSafeAreaSimulation(event.value ~= 'device' and event.value or nil)
                end}},
                ui.table{id = 'rects', columns = {{text = 'Area', width = 200}, {text = 'In design units'}}},
            },
            ui.card{grow = 1, gap = 12, align = 'stretch', ui.sectionTitle{text = 'The device'}, ui.spacer{id = 'preview', grow = 1}},
        }},
    }
    self:listen('windowSafeAreaChanged', function()
        self.changes = self.changes + 1
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
    local n = areas.number
    local rows = {
        {id = 'visible', cells = {'Visible screen', string.format('%s by %s at %s, %s', n(visible.width), n(visible.height), n(visible.x), n(visible.y))}},
        {id = 'safe', cells = {'Safe area', string.format('%s by %s at %s, %s', n(safe.width), n(safe.height), n(safe.x), n(safe.y))}},
        {id = 'insets', cells = {'Insets', string.format('Top %s, right %s, bottom %s, left %s', n(top), n(right), n(bottom), n(left))}},
    }
    local key = rows[1].cells[2] .. rows[2].cells[2] .. rows[3].cells[2]
    if key ~= self.rects then
        self.rects = key
        self:set('rects', {rows = rows})
    end
    local moves = self.changes == 0 and 'has not moved yet' or (self.changes == 1 and 'moved once' or 'moved ' .. self.changes .. ' times')
    self:status(string.format('The overlay is %s, and the safe area %s.', ui.safeAreaVisible() and 'shown' or 'hidden', moves))
end

-- The preview draws the simulated device with the safe area the engine reports, over the card that holds its room.
function DebugOverlay:render()
    local bounds = self.gui and self.gui:bounds('preview')
    if not bounds then
        return
    end
    graphics2d.beginScreen({order = 1})
    areas.drawPreview(bounds, areas.kinds[self.device] or 'island')
end

return DebugOverlay
