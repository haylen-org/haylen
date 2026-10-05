-- The function `viewport.setSafeAreaSimulation` replaces the safe area of the device while the app runs, with the insets of a known device in the orientation of the window or with custom insets in window points.
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Test = require('harness.test')
local areas = require('categories.safe-area.areas')

local Simulations = haylen.class('Simulations', Test)

Simulations.sides = {'top', 'right', 'bottom', 'left'}

function Simulations:init(entry)
    Simulations.super.init(self, entry)
    self.custom = {top = 60, right = 0, bottom = 40, left = 120}
end

-- The HUD is a GUI under the frame whose parts are anchored to the left and right edges of the safe area, so it moves with every device.
function Simulations:enter()
    self.simulation = areas.simulate()
    local current = viewport.safeAreaSimulation()
    self.device = current == nil and 'device' or (type(current) == 'string' and current or 'custom')
    self:frame{
        hint = 'The HUD is anchored to the safe area, so it moves with every device. Custom insets are window points: top, right, bottom and left. Leaving the test puts the safe area of the device back.',
        focus = 'device',
        content = {ui.row{ui.spacer{grow = 1}, ui.card{width = 900, gap = 16, children = self:controls()}, ui.spacer{grow = 1}}},
    }
    ui.mount(ui.stack{
        ui.column{anchor = 'left', margin = 16, width = 360, gap = 16,
            ui.panel{gap = 8, ui.label{text = 'Captain Ana'}, ui.progress{value = 0.7, tone = 'success', text = 'Health'}},
            ui.circularProgress{size = 140, value = 0.35, variant = 'cooldown', tone = 'information', text = 'Dash'},
        },
        ui.column{anchor = 'right', margin = 16, width = 300, gap = 16,
            ui.panel{height = 160, ui.label{text = 'Map', textAlign = 'center', align = 'stretch'}},
            ui.row{gap = 16, ui.button{text = 'Jump', variant = 'primary', focusable = false}, ui.button{text = 'Attack', variant = 'destructive', focusable = false}},
        },
    }, {layer = -1, owner = self})
end

function Simulations:exit()
    viewport.setSafeAreaSimulation(self.simulation)
    Simulations.super.exit(self)
end

function Simulations:apply()
    if self.device == 'custom' then
        local custom = self.custom
        viewport.setSafeAreaSimulation({custom.top, custom.right, custom.bottom, custom.left})
        return
    end
    viewport.setSafeAreaSimulation(self.device ~= 'device' and self.device or nil)
end

function Simulations:controls()
    local devices = {}
    for index, device in ipairs(areas.devices) do
        devices[index] = device
    end
    devices[#devices + 1] = {id = 'custom', text = 'Custom insets'}
    local steppers = {}
    for index, side in ipairs(Simulations.sides) do
        steppers[index] = ui.column{gap = 4, grow = 1, ui.label{text = side:sub(1, 1):upper() .. side:sub(2), font = 'caption', color = 'textMuted'}, ui.stepper{id = 'inset-' .. side, value = self.custom[side], min = 0, max = 200, step = 20, onChange = function(event)
            self.custom[side] = event.value
            if self.device == 'custom' then
                self:apply()
            end
        end}}
    end
    return {
        ui.formField{label = 'Device', ui.combo{id = 'device', items = devices, selected = self.device, onChange = function(event)
            self.device = event.value
            self:apply()
        end}},
        ui.row{gap = 12, children = steppers},
    }
end

local function describe(simulation)
    if simulation == nil then
        return 'The safe area of this device'
    end
    if type(simulation) == 'string' then
        return 'Device "' .. simulation .. '"'
    end
    return string.format('Insets %g, %g, %g, %g', simulation[1], simulation[2], simulation[3], simulation[4])
end

function Simulations:update(dt)
    Simulations.super.update(self, dt)
    local safe = viewport.safeRect()
    self:status(string.format('%s, safe area %.0f, %.0f, %.0f x %.0f.', describe(viewport.safeAreaSimulation()), safe.x, safe.y, safe.width, safe.height))
end

function Simulations:render()
    areas.drawBands()
end

return Simulations
