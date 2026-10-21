-- The function `viewport.setSafeAreaSimulation` replaces the safe area of the device while the app runs, with the insets of a known device in the orientation of the window or with custom insets in window points. The frame of the test follows the simulated safe area, and the preview holds a game HUD on a drawing of the device, inside the safe area the engine reports.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Device = require('categories.safe-area.device')
local Test = require('harness.test')
local areas = require('categories.safe-area.areas')

local Simulations = haylen.class('Simulations', Test)

Simulations.code = [[
viewport.setSafeAreaSimulation('iphoneDynamicIsland')  -- Or `iphoneNotch`, `ipad`, `androidGestureBar` and `television`.
viewport.setSafeAreaSimulation({60, 0, 40, 120})  -- Top, right, bottom and left in window points.
viewport.setSafeAreaSimulation(nil)  -- The safe area of the device again.
print(viewport.safeAreaSimulation(), viewport.safeRect())]]

Simulations.sides = {'top', 'right', 'bottom', 'left'}

function Simulations:init(entry)
    Simulations.super.init(self, entry)
    self.custom = {top = 60, right = 0, bottom = 40, left = 120}
end

function Simulations:enter()
    self.simulation = areas.simulate()
    local current = viewport.safeAreaSimulation()
    self.device = current == nil and 'device' or (type(current) == 'string' and current or 'custom')
    local visible = viewport.visibleRect()
    self:frame{
        hint = 'The HUD on the device sits in the safe area the engine reports, so it moves with every device. Custom insets are window points: top, right, bottom and left. Leaving the test puts the safe area of the device back.',
        code = Simulations.code,
        focus = 'device',
        fixed = true,
        content = {ui.row{grow = 1, gap = 24, align = 'stretch',
            ui.stack{id = 'room', grow = 1, align = 'stretch',
                ui.stack{id = 'screen', aspectRatio = visible.width / visible.height, align = 'stretch', margin = {56, 72}, ui.stack{id = 'safe', align = 'stretch', self:hud()}},
            },
            ui.card{width = 560, gap = 16, align = 'start', children = self:controls()},
        }},
    }
    self:refresh()
end

-- A game HUD in the safe area of the preview: the captain and her health at the top left, the map at the top right and the buttons at the bottom right.
function Simulations:hud()
    return ui.stack{align = 'stretch',
        ui.panel{anchor = 'topLeft', anchorTo = 'parent', margin = 12, padding = {8, 12}, gap = 6,
            ui.row{gap = 8, ui.avatar{image = 'interface/images/avatar-ana.png', size = 40}, ui.label{text = 'Captain Ana', font = 'caption'}},
            ui.progress{value = 0.7, tone = 'success', width = 180},
        },
        ui.image{image = 'interface/images/island-map.png', anchor = 'topRight', anchorTo = 'parent', margin = 12, width = 150, height = 94, fit = 'cover', radius = 10},
        ui.row{anchor = 'bottomRight', anchorTo = 'parent', margin = 12, gap = 8, ui.chip{text = 'Jump', selected = true}, ui.chip{text = 'Attack'}},
    }
end

function Simulations:exit()
    viewport.setSafeAreaSimulation(self.simulation)
    Simulations.super.exit(self)
end

function Simulations:apply()
    if self.device == 'custom' then
        local custom = self.custom
        viewport.setSafeAreaSimulation({custom.top, custom.right, custom.bottom, custom.left})
    else
        viewport.setSafeAreaSimulation(self.device ~= 'device' and self.device or nil)
    end
    self:refresh()
end

function Simulations:controls()
    local devices = {}
    for index, device in ipairs(areas.devices) do
        devices[index] = device
    end
    devices[#devices + 1] = {id = 'custom', text = 'Custom insets'}
    local steppers = {}
    for index, side in ipairs(Simulations.sides) do
        steppers[index] = ui.formField{label = side:sub(1, 1):upper() .. side:sub(2), grow = 1, ui.stepper{id = 'inset-' .. side, value = self.custom[side], min = 0, max = 200, step = 20, onChange = function(event)
            if self.device == 'custom' then
                self.custom[side] = event.value
                self:apply()
            end
        end}}
    end
    return {
        ui.sectionTitle{text = 'Simulation'},
        ui.formField{label = 'Device', ui.combo{id = 'device', items = devices, selected = self.device, onChange = function(event)
            self.device = event.value
            self:apply()
        end}},
        ui.grid{columns = 2, gap = 16, children = steppers},
        ui.label{id = 'note', text = '', color = 'textMuted'},
    }
end

-- The steppers show the insets of the device in window points and edit them only for custom insets.
function Simulations:refresh()
    local custom = self.device == 'custom'
    local top, right, bottom, left = areas.insets()
    local perUnit = viewport.pixelsPerUnit()
    local points = perUnit / window.dpiScale()
    local values = {top = top * points, right = right * points, bottom = bottom * points, left = left * points}
    for _, side in ipairs(Simulations.sides) do
        self:set('inset-' .. side, {value = custom and self.custom[side] or math.floor(values[side] + 0.5), enabled = custom})
    end
    self:set('note', {text = custom and 'Step the insets to move the safe area.' or 'Pick "Custom insets" to edit them.'})
end

local function describe(simulation)
    if simulation == nil then
        return 'This device reports its own safe area'
    end
    if type(simulation) == 'string' then
        for _, device in ipairs(areas.devices) do
            if device.id == simulation then
                return 'The simulation shows ' .. device.text:sub(1, 1):lower() .. device.text:sub(2)
            end
        end
    end
    return 'The simulation uses custom insets'
end

function Simulations:update(dt)
    Simulations.super.update(self, dt)
    local safe = viewport.safeRect()
    self:status(string.format('%s, whose safe area is %s by %s units.', describe(viewport.safeAreaSimulation()), areas.number(safe.width), areas.number(safe.height)))
    -- The safe area of the preview takes the insets the engine reports, scaled to the screen of the preview.
    local screen = self.gui:bounds('screen')
    if screen then
        local visible = viewport.visibleRect()
        local scale = screen.width / visible.width
        local top, right, bottom, left = areas.insets()
        local margins = {math.floor(top * scale), math.floor(right * scale), math.floor(bottom * scale), math.floor(left * scale)}
        local key = table.concat(margins, ' ')
        if key ~= self.margins then
            self.margins = key
            self:set('safe', {margin = margins})
        end
    end
end

-- The device of the simulation holds the screen of the preview, with the bands outside its safe area shaded.
function Simulations:render()
    local screen = self.gui and self.gui:bounds('screen')
    local safe = self.gui and self.gui:bounds('safe')
    if not screen or not safe then
        return
    end
    graphics2d.beginScreen()
    local kind = areas.kinds[self.device] or 'island'
    local device, radius = Device.around(kind, {screen.x, screen.y, screen.width, screen.height}, kind ~= 'tv' and kind ~= 'tablet')
    device.screen = '#FF1E3A5F'
    Device.draw(device)
    graphics2d.pushMask(function()
        graphics2d.drawShape({screen.x, screen.y, screen.width, screen.height}, {radius = radius, color = '#FFFFFFFF'})
    end, {layer = 3})
    graphics2d.drawRect({screen.x, screen.y, screen.width, screen.height}, '#40F0473E')
    graphics2d.drawRect({safe.x, safe.y, safe.width, safe.height}, '#FF1E3A5F', {layer = 1})
    graphics2d.popMask()
    graphics2d.drawShape({safe.x, safe.y, safe.width, safe.height}, {radius = 8, color = '#00000000', borderWidth = 2, borderColor = '#CC3AA8E0', layer = 5})
end

return Simulations
