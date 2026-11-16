-- The controls along the bottom: the climate of the driver and of the passenger, each with its temperature, its automatic mode and the heating of its seat, and the dock with home, navigation, music, phone, apps and settings. They are the GUI of the dashboard, so a mouse, a finger, the keyboard, a gamepad and a TV remote all reach them, and a landscape screen shows them in one bar while a portrait one shows the dock over the climate.
local m = require('haylen.math')
local ui = require('haylen.ui')

local Style = require('style')
local Text = require('text')

local Controls = {}
Controls.__index = Controls

Controls.dock = {
    {id = 'home', text = 'Home'},
    {id = 'navigation', text = 'Navigation'},
    {id = 'music', text = 'Music'},
    {id = 'phone', text = 'Phone'},
    {id = 'apps', text = 'Apps'},
    {id = 'settings', text = 'Settings'},
}

-- The temperatures the climate takes, in degrees Celsius, the step of a press, and the levels of the seat heating.
Controls.lowest, Controls.highest, Controls.step = 16, 28, 0.5
Controls.seatLevels = 3

-- The bar paints the panel of the dashboard edge to edge, and its buttons keep their round corners.
Controls.barStyle = {colors = {panel = '#F20A1122'}, metrics = {panelRadius = 36}}
Controls.roundStyle = {metrics = {controlRadius = 36, controlPaddingX = 0, iconSize = 36}}

function Controls.new(owner)
    local self = setmetatable({}, Controls)
    self.zones = {
        {id = 'driver', setpoint = 22, seat = 2, automatic = true},
        {id = 'passenger', setpoint = 22, seat = 0, automatic = true},
    }
    self.cabinTemperature = 23.4
    self.selected = 'home'
    self.gui = ui.mount(ui.column{id = 'bar', anchor = 'stretchBottom', gap = 0}, {owner = owner})
    return self
end

-- Builds the bar for the orientation of the screen, keeping the focus on the control that has it.
function Controls:build(landscape)
    if self.landscape == landscape then
        return
    end
    self.landscape = landscape
    local space = 16
    local bar
    if landscape then
        bar = ui.panel{id = 'panel', gap = space, padding = {14, 24}, style = Controls.barStyle,
            ui.row{gap = space, align = 'stretch', self:zone(self.zones[1]), ui.spacer{grow = 1}, self:dockRow(), ui.spacer{grow = 1}, self:zone(self.zones[2])},
        }
    else
        bar = ui.panel{id = 'panel', gap = 10, padding = {14, 18}, style = Controls.barStyle,
            ui.row{gap = 0, align = 'stretch', justify = 'center', self:dockRow()},
            ui.row{gap = space, align = 'stretch', self:zone(self.zones[1]), ui.spacer{grow = 1}, self:zone(self.zones[2])},
        }
    end
    self.gui:replaceChildren('bar', {bar})
    for _, zone in ipairs(self.zones) do
        self:showZone(zone)
    end
end

-- The dock: one button per app, with the one that shows lit in the violet of the accent.
function Controls:dockRow()
    local row = {gap = 6, align = 'center'}
    for _, item in ipairs(Controls.dock) do
        local selected = item.id == self.selected
        row[#row + 1] = ui.button{
            id = 'dock-' .. item.id,
            text = item.text,
            icon = 'icons/' .. item.id .. '.svg',
            iconPlacement = 'top',
            variant = selected and 'primary' or 'toolbar',
            iconColor = selected and 'onAccent' or 'text',
            width = 156,
            height = 112,
            accessibilityLabel = item.text,
            onClick = function()
                self:select(item.id)
            end,
        }
    end
    return ui.row(row)
end

-- The climate of one seat: down, the temperature over its mode, up, and the heating of the seat with a dot for each level.
function Controls:zone(zone)
    local prefix = zone.id .. '-'
    local seat = ui.button{id = prefix .. 'seat', icon = 'icons/seat.svg', iconColor = 'text', variant = 'toolbar', width = 88, height = 88, accessibilityLabel = 'Seat heating', style = Controls.roundStyle,
        onClick = function()
            self:heatSeat(zone)
        end}
    local dots = {gap = 6, justify = 'center'}
    for level = 1, Controls.seatLevels do
        dots[#dots + 1] = ui.panel{id = prefix .. 'dot' .. level, width = 14, height = 6, padding = 0, style = {colors = {panel = Style.color.faint}, metrics = {panelRadius = 3}}}
    end
    local temperature = ui.column{gap = 0, width = 150, align = 'center',
        ui.label{id = prefix .. 'temperature', text = '', font = 'title', textAlign = 'center', align = 'stretch'},
        ui.button{id = prefix .. 'auto', text = 'AUTO', variant = 'link', height = 40, accessibilityLabel = 'Automatic climate',
            onClick = function()
                zone.automatic = not zone.automatic
                self:showZone(zone)
            end},
    }
    local down = ui.button{id = prefix .. 'down', icon = 'icons/minus.svg', iconColor = 'text', width = 72, height = 72, accessibilityLabel = 'Cooler', style = Controls.roundStyle,
        onClick = function()
            self:change(zone, -Controls.step)
        end}
    local up = ui.button{id = prefix .. 'up', icon = 'icons/plus.svg', iconColor = 'text', width = 72, height = 72, accessibilityLabel = 'Warmer', style = Controls.roundStyle,
        onClick = function()
            self:change(zone, Controls.step)
        end}
    local seatColumn = ui.column{gap = 4, align = 'center', seat, ui.row(dots)}
    if zone.id == 'driver' then
        return ui.row{gap = 10, align = 'center', down, temperature, up, seatColumn}
    end
    return ui.row{gap = 10, align = 'center', seatColumn, down, temperature, up}
end

-- Steps the heating of a seat to its next level, and from the highest back to off.
function Controls:heatSeat(zone)
    zone.seat = (zone.seat + 1) % (Controls.seatLevels + 1)
    self:showZone(zone)
end

function Controls:change(zone, delta)
    zone.setpoint = m.clamp(zone.setpoint + delta, Controls.lowest, Controls.highest)
    self:showZone(zone)
end

function Controls:showZone(zone)
    local prefix = zone.id .. '-'
    self.gui:set(prefix .. 'temperature', {text = Text.format('%.1f°C', zone.setpoint, 0.1)})
    self.gui:set(prefix .. 'auto', {style = {colors = {accentText = zone.automatic and '#FFB49CFF' or Style.color.faint}}})
    for level = 1, Controls.seatLevels do
        self.gui:set(prefix .. 'dot' .. level, {style = {colors = {panel = level <= zone.seat and Style.color.orange or Style.color.faint}, metrics = {panelRadius = 3}}})
    end
end

-- Lights the app of the dock that was chosen.
function Controls:select(id)
    if self.selected == id then
        return
    end
    local previous = self.selected
    self.selected = id
    for _, item in ipairs({previous, id}) do
        local selected = item == id
        self.gui:set('dock-' .. item, {variant = selected and 'primary' or 'toolbar', iconColor = selected and 'onAccent' or 'text'})
    end
end

-- The cabin warms or cools toward the mean of the two setpoints.
function Controls:update(dt)
    local wanted = (self.zones[1].setpoint + self.zones[2].setpoint) * 0.5
    self.cabinTemperature = self.cabinTemperature + (wanted - self.cabinTemperature) * math.min(1, dt * 0.05)
end

function Controls:cabin()
    return self.cabinTemperature
end

-- The top of the bar in design units, where the drawings of the dashboard end, once the bar drew.
function Controls:top(fallback)
    local area = self.gui:bounds('panel')
    return area and area.y or fallback
end

return Controls
