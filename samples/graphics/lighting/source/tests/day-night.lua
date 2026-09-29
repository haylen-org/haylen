-- Day and night written in Lua: the clock blends the ambient and sky colors between keyframes, turns the sun and switches the street lamps and windows on at dusk.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local m = require('haylen.math')
local ui = require('haylen.ui')

local sample = require('sample')

local DayNight = haylen.class('DayNight', sample.Test)

DayNight.hints = 'The clock runs by itself. Jump to a time of day or change how many hours pass per second.'

-- Hours of the day with the ambient light and the sky color the clock blends between.
DayNight.keys = {
    {hour = 0, ambient = '#FF1C2244', sky = '#FF0A0E24'},
    {hour = 5, ambient = '#FF262A50', sky = '#FF141A3A'},
    {hour = 6.5, ambient = '#FFD8A080', sky = '#FFF0A070'},
    {hour = 9, ambient = '#FFFFF6EA', sky = '#FF8CC8F0'},
    {hour = 16, ambient = '#FFFFF6EA', sky = '#FF8CC8F0'},
    {hour = 18.5, ambient = '#FFE08060', sky = '#FFF08050'},
    {hour = 20, ambient = '#FF2E3060', sky = '#FF1A1C44'},
    {hour = 24, ambient = '#FF1C2244', sky = '#FF0A0E24'},
}

DayNight.houses = {-760, -300, 200, 640}

function DayNight:init(entry)
    DayNight.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.hour = 17
    self.rate = 1
    self.running = true
    self.night = false
    self.ambient, self.sky = DayNight.colorsAt(self.hour)
    self.sun = lighting2d.newLight({type = 'directional', color = '#FFFFF0D0', shadows = true, shadowFilter = 'pcf5'})
    self.lamps, self.occluders = {}, {}
    for index, x in ipairs(DayNight.houses) do
        self.lamps[index] = lighting2d.newLight({x = x + 330, y = 140, radius = 300, color = '#FFFFC070'})
        self.occluders[index] = lighting2d.newOccluder({points = {x, -60, x + 280, -60, x + 280, 240, x, 240}, cull = 'counterClockwise'})
    end
end

function DayNight:controls()
    local times = {{'Dawn', 6}, {'Noon', 12}, {'Dusk', 18.5}, {'Midnight', 0}}
    local buttons = {}
    for index, time in ipairs(times) do
        buttons[index] = ui.button{text = time[1], onClick = function()
            self.hour = time[2]
        end}
    end
    return {
        ui.toggle{align = 'stretch', text = 'Clock runs', checked = true, onChange = function(event)
            self.running = event.checked
        end},
        ui.formField{label = 'Hours per second', ui.slider{min = 0.1, max = 4, value = self.rate, showValue = true, onChange = function(event)
            self.rate = event.value
        end}},
        ui.grid{columns = 2, gap = 8, children = buttons},
    }
end

-- Returns the ambient and sky colors of an hour, blended between the two keyframes around it.
function DayNight.colorsAt(hour)
    local keys = DayNight.keys
    for index = 2, #keys do
        local after = keys[index]
        if hour <= after.hour then
            local before = keys[index - 1]
            local t = m.inverseLerp(before.hour, after.hour, hour)
            return m.color(before.ambient):lerp(m.color(after.ambient), t), m.color(before.sky):lerp(m.color(after.sky), t)
        end
    end
    return m.color(keys[1].ambient), m.color(keys[1].sky)
end

function DayNight:update(dt)
    DayNight.super.update(self, dt)
    if self.running then
        self.hour = (self.hour + dt * self.rate) % 24
    end
    self.ambient, self.sky = DayNight.colorsAt(self.hour)

    -- The sun rises at six on the left and sets at eighteen on the right.
    local arc = (self.hour - 6) / 12
    self.sun.enabled = arc > 0 and arc < 1
    self.sun.rotation = arc * math.pi
    self.sun.intensity = math.max(0, math.sin(arc * math.pi)) * 0.9
    self.night = self.hour < 6.5 or self.hour > 18
    for _, lamp in ipairs(self.lamps) do
        lamp.enabled = self.night
        lamp.intensity = 1.2 * lighting2d.flicker(haylen.time(), {amount = 0.05})
    end
    self:setStatus(string.format('%02d:%02d, ambient %s, %s', math.floor(self.hour), math.floor(self.hour % 1 * 60), self.ambient:toHex(), self.night and 'lamps on' or 'lamps off'))
end

function DayNight:drawSky()
    graphics2d.drawRect({-1600, -900, 3200, 820}, self.sky, {unshaded = true})
    local arc = (self.hour - 6) / 12
    if arc > 0 and arc < 1 then
        graphics2d.drawCircle(-800 + arc * 1600, -120 - math.sin(arc * math.pi) * 520, 60, '#FFFFF0A0', {layer = 1, unshaded = true})
    else
        local night = ((self.hour + 6) % 24) / 12
        graphics2d.drawCircle(-800 + night * 1600, -120 - math.sin(night * math.pi) * 460, 44, '#FFE0E8FF', {layer = 1, unshaded = true})
    end
end

function DayNight:render()
    graphics2d.beginWorld(self.camera, {ambientLight = self.ambient})
    self:drawSky()
    graphics2d.drawRect({-1600, -80, 3200, 1000}, '#FF5A8A4A')
    graphics2d.drawRect({-1600, 250, 3200, 160}, '#FF6A6A70', {layer = 1})
    for index, x in ipairs(DayNight.houses) do
        graphics2d.drawRect({x, -60, 280, 300}, '#FFB09070', {layer = 2})
        graphics2d.drawPolygon({{x - 20, -60}, {x + 140, -180}, {x + 300, -60}}, '#FF8A4030', {layer = 2})
        for column = 0, 1 do
            graphics2d.drawRect({x + 40 + column * 140, 20, 60, 70}, self.night and '#FFFFD27A' or '#FF3A4A60', {layer = 3, emission = self.night and 1 or 0})
        end
        graphics2d.drawRect({x + 324, 150, 12, 110}, '#FF3A3A40', {layer = 2})
        graphics2d.drawCircle(x + 330, 140, 16, self.night and '#FFFFE0A0' or '#FF707070', {layer = 3, emission = self.night and 1.5 or 0})
        graphics2d.drawOccluder(self.occluders[index])
        graphics2d.drawLight(self.lamps[index])
    end
    graphics2d.drawLight(self.sun)
end

return DayNight
