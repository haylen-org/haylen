-- Day and night written in Lua: the clock blends the ambient light and the sky between the colors of the hours, carries the sun and the moon across the sky, brings out the stars and lights the street lamps and the windows of the village at dusk.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local m = require('haylen.math')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Town = require('categories.lighting.town')

local DayNight = haylen.class('DayNight', LightingTest)

DayNight.code = [[
local sun = lighting2d.newLight({type = 'directional', color = '#FFFFF0D0'})
local arc = (hour - 6) / 12  -- The sun rises at 6 and sets at 18.
sun.rotation, sun.intensity = arc * math.pi, math.max(0, math.sin(arc * math.pi))
local ambient = dusk:lerp(night, t)  -- The color between two hours.
graphics2d.beginWorld(camera, {ambientLight = ambient})
graphics2d.drawRect(window, '#FFFFC870', {emission = glow})  -- Lit at night.]]

-- Hours of the day with the ambient light and the colors of the sky at the top and at the horizon, which the clock blends between.
DayNight.keys = {
    {hour = 0, ambient = '#FF1C2244', zenith = '#FF070B22', horizon = '#FF1C2650'},
    {hour = 5, ambient = '#FF262A50', zenith = '#FF0E1438', horizon = '#FF34386A'},
    {hour = 6.5, ambient = '#FFD8A080', zenith = '#FF4A5AA0', horizon = '#FFF6A878'},
    {hour = 9, ambient = '#FFFFF6EA', zenith = '#FF2F7FD8', horizon = '#FFA8DCF8'},
    {hour = 16, ambient = '#FFFFF6EA', zenith = '#FF2F7FD8', horizon = '#FFA8DCF8'},
    {hour = 18.5, ambient = '#FFE08060', zenith = '#FF44488E', horizon = '#FFF8925E'},
    {hour = 20, ambient = '#FF2E3060', zenith = '#FF10163E', horizon = '#FF2E3264'},
    {hour = 24, ambient = '#FF1C2244', zenith = '#FF070B22', horizon = '#FF1C2650'},
}

DayNight.horizon = -110
DayNight.ground = 250
DayNight.houses = {
    {name = 'cottage', x = -1120}, {name = 'stone-house', x = -700}, {name = 'cottage', x = -250},
    {name = 'stone-house', x = 230}, {name = 'cottage', x = 650}, {name = 'stone-house', x = 1080},
}
DayNight.houseScale = 0.6
DayNight.lampPosts = {-880, -505, 18, 411, 893}
DayNight.lampHeight = 300
DayNight.times = {{'Dawn', 6}, {'Noon', 12}, {'Dusk', 18.5}, {'Midnight', 0}}

function DayNight:init(entry)
    DayNight.super.init(self, entry)
    self.town = Town()
    self.hour = 17
    self.rate = 1
    self.running = true
    self.glow = 0
    self.colors = DayNight.colorsAt(self.hour)
    self.ambientLight = self.colors.ambient
    self.sun = lighting2d.newLight({type = 'directional', color = '#FFFFF0D0'})
    self.lamps = {}
    for index = 1, #DayNight.lampPosts do
        self.lamps[index] = lighting2d.newLight({radius = 340, color = '#FFFFC070', intensity = 1.2})
    end
    local random = m.random(13)
    self.stars = {}
    for index = 1, 90 do
        self.stars[index] = {x = random:range(-1400, 1400), y = random:range(-540, DayNight.horizon + 30), size = random:range(1.5, 4), phase = random:range(0, m.tau)}
    end
end

function DayNight:enter()
    local buttons = {}
    for index, time in ipairs(DayNight.times) do
        buttons[index] = ui.button{id = time[1]:lower(), text = time[1], onClick = function()
            self.hour = time[2]
        end}
    end
    self:frame{
        hint = 'The clock runs by itself. Jump to a time of day or change how many hours pass each second.',
        code = DayNight.code,
        controls = {
            ui.toggle{id = 'running', text = 'Clock runs', checked = true, onChange = function(event)
                self.running = event.checked
            end},
            ui.formField{label = 'Hours per second', ui.slider{id = 'rate', min = 0.1, max = 4, value = self.rate, showValue = true, onChange = function(event)
                self.rate = event.value
            end}},
            ui.grid{columns = 2, gap = 8, children = buttons},
        },
        focus = 'running',
    }
end

-- Returns the ambient light and the sky colors of an hour, blended between the two keys around it.
function DayNight.colorsAt(hour)
    local keys = DayNight.keys
    for index = 2, #keys do
        local after = keys[index]
        if hour <= after.hour then
            local before = keys[index - 1]
            local t = m.inverseLerp(before.hour, after.hour, hour)
            local function blend(field)
                return m.color(before[field]):lerp(m.color(after[field]), t)
            end
            return {ambient = blend('ambient'), zenith = blend('zenith'), horizon = blend('horizon')}
        end
    end
end

-- How much the lamps and the windows glow: off by day, rising from 18 to 19 and fading from 6 to 7.
function DayNight.glowAt(hour)
    if hour >= 7 and hour <= 18 then
        return 0
    end
    if hour > 18 and hour < 19 then
        return m.smoothstep(18, 19, hour)
    end
    if hour > 6 and hour < 7 then
        return 1 - m.smoothstep(6, 7, hour)
    end
    return 1
end

function DayNight:update(dt)
    DayNight.super.update(self, dt)
    if self.running then
        self.hour = (self.hour + dt * self.rate) % 24
    end
    self.colors = DayNight.colorsAt(self.hour)
    self.ambientLight = self.colors.ambient
    self.glow = DayNight.glowAt(self.hour)

    -- The sun rises at six on the left and sets at eighteen on the right, and its light travels away from it.
    local arc = (self.hour - 6) / 12
    self.sun.enabled = arc > 0 and arc < 1
    self.sun.rotation = arc * math.pi
    self.sun.intensity = math.max(0, math.sin(arc * math.pi)) * 0.9
    for _, lamp in ipairs(self.lamps) do
        lamp.enabled = self.glow > 0
        lamp.intensity = 1.2 * self.glow * lighting2d.flicker(haylen.elapsed(), {amount = 0.05})
    end
    local text = self.glow > 0.5 and 'The street lamps and the windows are lit.' or self.glow > 0 and 'The lamps are coming on.' or 'The lamps are off.'
    self:status(string.format('It is %02d:%02d. %s', math.floor(self.hour), math.floor(self.hour % 1 * 60), text))
end

-- The sun crosses the sky from six to eighteen and the moon from eighteen to six, each on an arc above the hills.
function DayNight:drawSky()
    local colors = self.colors
    self.town:sky(colors.zenith, colors.horizon, DayNight.horizon + 120)
    local starlight = math.max(0, self.glow * 1.2 - 0.2)
    if starlight > 0 then
        local time = haylen.elapsed()
        for _, star in ipairs(self.stars) do
            local twinkle = 0.6 + 0.4 * math.sin(time * 2 + star.phase)
            graphics2d.drawCircle(star.x, star.y, star.size, m.color(1, 1, 0.92, starlight * twinkle), {layer = -18, unshaded = true})
        end
    end

    local arc = (self.hour - 6) / 12
    local body, place = 'sun', arc
    if arc <= 0 or arc >= 1 then
        body, place = 'moon', ((self.hour + 6) % 24) / 12
    end
    local x, y = -950 + place * 1900, DayNight.horizon + 80 - math.sin(place * math.pi) * 520
    local halo = body == 'sun' and '#90FFE08A' or '#60C8D8FF'
    graphics2d.draw(graphics2d.lightTexture(), x, y, {width = 520, height = 520, color = halo, blend = 'additive', layer = -17, unshaded = true})
    self.town:piece(body, x, y, 0.55, {layer = -16, unshaded = true})
end

function DayNight:draw(area)
    self:drawSky()
    self.town:land(DayNight.horizon, DayNight.ground)
    for _, house in ipairs(DayNight.houses) do
        self.town:house(house.name, house.x, DayNight.ground + 12, DayNight.houseScale, 2, self.glow)
    end
    for index, x in ipairs(DayNight.lampPosts) do
        local lanternX, lanternY = self.town:lampPost(x, DayNight.ground + 30, DayNight.lampHeight, 4, self.glow)
        self.lamps[index].x, self.lamps[index].y = lanternX, lanternY
        graphics2d.drawLight(self.lamps[index])
    end
    graphics2d.drawLight(self.sun)
end

return DayNight
