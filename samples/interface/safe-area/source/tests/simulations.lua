-- Device simulations: viewport.setSafeAreaSimulation replaces the safe area of the device while the app runs, with the insets of a known device in the orientation of the window or with custom insets in window points.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local sample = require('sample')

local Simulations = haylen.class('Simulations', sample.Test)

Simulations.hints = 'The HUD is anchored to the safe area, so it moves with every device. Custom insets are window points, top, right, bottom and left. The device you pick stays for the other tests of this sample.'
Simulations.focus = 'device'

local kSides = {'top', 'right', 'bottom', 'left'}

function Simulations:init(entry)
    Simulations.super.init(self, entry)
    local current = viewport.safeAreaSimulation()
    self.device = current == nil and 'device' or (type(current) == 'string' and current or 'custom')
    self.custom = {top = 60, right = 0, bottom = 40, left = 120}
end

function Simulations:apply()
    if self.device == 'custom' then
        local custom = self.custom
        viewport.setSafeAreaSimulation({custom.top, custom.right, custom.bottom, custom.left})
    else
        viewport.setSafeAreaSimulation(self.device ~= 'device' and self.device or nil)
    end
end

function Simulations:controls()
    local devices = {}
    for index, device in ipairs(sample.devices) do
        devices[index] = device
    end
    devices[#devices + 1] = {id = 'custom', text = 'Custom insets'}
    local steppers = {}
    for index, side in ipairs(kSides) do
        steppers[index] = ui.column{gap = 4, grow = 1, ui.label{text = side, font = 'caption', color = 'textMuted'}, ui.stepper{id = 'inset-' .. side, value = self.custom[side], min = 0, max = 200, step = 20, onChange = function(event)
            self.custom[side] = event.value
            if self.device == 'custom' then
                self:apply()
            end
        end}}
    end
    return {
        ui.settingsRow{label = 'Device', ui.combo{id = 'device', width = 480, items = devices, selected = self.device, onChange = function(event)
            self.device = event.value
            self:apply()
        end}},
        ui.row{gap = 12, children = steppers},
    }
end

-- A game HUD whose parts are anchored to the corners and edges of the safe area.
function Simulations:started()
    self.hud = sample.mount(self, ui.stack{
        ui.panel{anchor = 'topLeft', margin = 16, width = 360, gap = 8, ui.label{text = 'Captain Ana'}, ui.progress{value = 0.7, tone = 'success', text = 'Health'}},
        ui.label{anchor = 'top', margin = 16, text = 'Day 3', font = 'title'},
        ui.panel{anchor = 'topRight', margin = 16, width = 220, height = 160, ui.label{text = 'Map', textAlign = 'center', align = 'stretch'}},
        ui.circularProgress{anchor = 'bottomLeft', margin = 24, size = 140, value = 0.35, style = 'cooldown', tone = 'information', text = 'Dash'},
        ui.row{anchor = 'bottomRight', margin = 24, gap = 16, ui.button{text = 'Jump', variant = 'primary', focusable = false}, ui.button{text = 'Attack', variant = 'destructive', focusable = false}},
    })
end

local function describe(simulation)
    if simulation == nil then
        return 'the safe area of this device'
    end
    if type(simulation) == 'string' then
        return simulation
    end
    return string.format('insets %g, %g, %g, %g', simulation[1], simulation[2], simulation[3], simulation[4])
end

function Simulations:update(dt)
    local safe = viewport.safeRect()
    self:setStatus(string.format('%s, safe area %.0f, %.0f, %.0f x %.0f', describe(viewport.safeAreaSimulation()), safe.x, safe.y, safe.width, safe.height))
end

function Simulations:render()
    graphics2d.beginScreen()
    sample.drawBands('#40FF4040')
    graphics2d.drawRectOutline(viewport.safeRect(), 3, '#FF3AA8E0')
end

return Simulations
