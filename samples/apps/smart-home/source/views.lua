-- The live pictures of the panel, each drawn every frame into a render target that a picture of the GUI shows, so they lay out, scroll and take the focus with the cards around them: the house under the sky of the hour, the dial of the thermostat, the camera at the front door and the energy of the day. Each target follows the size its picture takes on the screen, in pixels, so it stays sharp on every screen.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Style = require('style')
local Text = require('text')

local Views = {}
Views.__index = Views

-- The pictures by the id of their node in the GUI, with the size their target starts at.
Views.list = {
    {id = 'house', name = '@house', width = 960, height = 600},
    {id = 'dial', name = '@dial', width = 640, height = 640},
    {id = 'camera', name = '@camera', width = 960, height = 540},
    {id = 'energy', name = '@energy', width = 960, height = 420},
}

function Views.new(images)
    local self = setmetatable({images = images, views = {}}, Views)
    for _, entry in ipairs(Views.list) do
        local view = {id = entry.id, name = entry.name, camera = graphics2d.newCamera()}
        self.views[entry.id] = view
        self:resize(view, entry.width, entry.height)
    end
    self.rect = {0, 0, 0, 0}
    self.shape = {}
    self.textStyle = {anchor = {0, 0}}
    self.drawOptions = {pivotX = 0, pivotY = 0}
    self.curve = {}
    self.usageLine = {}
    self.indices = {}
    for slot = 1, 97 do
        self.curve[slot * 2 - 1] = {x = 0, y = 0, color = '#FFFFFFFF', u = 0, v = 0}
        self.curve[slot * 2] = {x = 0, y = 0, color = '#FFFFFFFF', u = 0, v = 1}
    end
    for slot = 1, 96 do
        local top, bottom = slot * 2 - 1, slot * 2
        local base = #self.indices
        self.indices[base + 1], self.indices[base + 2], self.indices[base + 3] = top, top + 2, bottom
        self.indices[base + 4], self.indices[base + 5], self.indices[base + 6] = top + 2, bottom + 2, bottom
    end
    for index = 1, 96 * 2 do
        self.usageLine[index] = 0
    end
    self.usageOptions = {width = 5, color = Style.color.coral, join = 'round', cap = 'round', count = 2}
    return self
end

function Views:resize(view, width, height)
    view.width, view.height = width, height
    view.target = graphics.newRenderTarget(width, height)
    view.camera.x, view.camera.y = width / 2, height / 2
    ui.setImage(view.name, view.target)
end

-- Gives every target the pixels its picture covers once the GUI drew it, when they changed by more than a pixel.
function Views:fit(gui)
    local density = viewport.pixelsPerUnit()
    for _, view in pairs(self.views) do
        local area = gui:bounds(view.id)
        if area and area.width > 0 then
            local width, height = math.floor(area.width * density + 0.5), math.floor(area.height * density + 0.5)
            if math.abs(width - view.width) > 1 or math.abs(height - view.height) > 1 then
                self:resize(view, width, height)
            end
        end
    end
end

function Views:shapeAt(x, y, width, height, radius, color, options)
    local rect, shape = self.rect, self.shape
    rect[1], rect[2], rect[3], rect[4] = x, y, width, height
    shape.kind = options and options.kind or 'rectangle'
    shape.radius = radius
    shape.color = color
    shape.borderWidth = options and options.borderWidth or 0
    shape.borderColor = options and options.borderColor or '#00000000'
    shape.softness = options and options.softness or 0
    shape.texture = options and options.texture or nil
    shape.startAngle = options and options.startAngle or 0
    shape.sweep = options and options.sweep or math.pi * 2
    graphics2d.drawShape(rect, shape)
end

function Views:text(text, x, y, size, color, anchorX, anchorY, bold)
    local style = self.textStyle
    style.size, style.color, style.bold = size, color, bold or false
    style.anchor[1], style.anchor[2] = anchorX, anchorY
    graphics2d.drawText(nil, text, x, y, style)
end

function Views:image(texture, x, y, width, height, color)
    local options = self.drawOptions
    options.width, options.height, options.color = width, height, color or '#FFFFFFFF'
    graphics2d.draw(texture, x, y, options)
end

function Views:draw(home)
    self:drawHouse(self.views.house, home)
    self:drawDial(self.views.dial, home)
    self:drawCamera(self.views.camera, home)
    self:drawEnergy(self.views.energy, home)
end

-- How much of the night shows, from 0 in the day to 1 in the dark, with the dusk in between.
function Views.darkness(share)
    local hour = share * 24
    if hour < 5 or hour > 21 then
        return 1
    end
    if hour < 7 then
        return (7 - hour) / 2
    end
    if hour > 19 then
        return (hour - 19) / 2
    end
    return 0
end

-- The house under the sky of the hour: the day fades into the dusk and the night, the sun or the moon crosses the sky, and the windows glow as the lights inside are on.
function Views:drawHouse(view, home)
    local width, height = view.width, view.height
    graphics2d.beginTarget(view.target, view.camera, Views.clearOptions)
    local dark = Views.darkness(home:dayShare())
    local dusk = math.min(dark, 1 - dark) * 2
    self:image(Style.skies.day, 0, 0, width, height)
    self:image(Style.skies.dusk, 0, 0, width, height, Views.alpha(dusk))
    self:image(Style.skies.night, 0, 0, width, height, Views.alpha(dark))

    -- The sun rises at six and sets at eight, and the moon crosses the rest of the day.
    local share = home:dayShare()
    local sunShare = (share * 24 - 6) / 14
    local body = sunShare > 0 and sunShare < 1 and sunShare or ((share * 24 + 24 - 20) % 24) / 10
    local x = width * (0.1 + 0.8 * body)
    local y = height * (0.62 - 0.45 * math.sin(body * math.pi))
    local radius = height * 0.07
    if sunShare > 0 and sunShare < 1 then
        self:shapeAt(x - radius * 3, y - radius * 3, radius * 6, radius * 6, 0, '#FFFFFFFF', Views.glowShape)
        self:shapeAt(x - radius, y - radius, radius * 2, radius * 2, 0, '#FFFFD27A', Views.ellipse)
    else
        self:shapeAt(x - radius, y - radius, radius * 2, radius * 2, 0, '#FFF3F1E6', Views.ellipse)
        self:shapeAt(x - radius * 0.55, y - radius * 1.15, radius * 2, radius * 2, 0, '#FF26315A', Views.ellipse)
    end

    -- The house stands on the bottom of the picture, and its windows glow at night when the rooms are lit.
    local houseHeight = height * 0.86
    local houseWidth = houseHeight * 1.2
    local left, top = (width - houseWidth) * 0.5, height - houseHeight
    self:image(self.images.house, left, top, houseWidth, houseHeight, dark > 0.5 and '#FFB8BDD6' or '#FFFFFFFF')
    local glow = dark * home:lightsOn() / 11
    if glow > 0.02 then
        for _, window in ipairs(Views.windows) do
            local size = houseWidth * 0.16
            local wx, wy = left + window[1] * houseWidth, top + window[2] * houseHeight
            self:shapeAt(wx - size, wy - size, size * 2, size * 2, 0, Views.alpha(glow), Views.glowShape)
        end
    end
end

-- The windows of the picture of the house, as shares of its width and height.
Views.windows = {{0.29, 0.43}, {0.56, 0.43}, {0.82, 0.41}, {0.72, 0.65}}

-- The colors of white with an alpha of every hundredth, made once.
Views.alphas = {}
function Views.alpha(value)
    local step = math.floor(math.max(0, math.min(1, value)) * 100 + 0.5)
    local color = Views.alphas[step]
    if not color then
        color = string.format('#%02XFFFFFF', math.floor(step * 2.55 + 0.5))
        Views.alphas[step] = color
    end
    return color
end

-- The dial of the thermostat: a ring that the target fills in the color of the mode, the mark of the room temperature on it, and the target in the middle.
function Views:drawDial(view, home)
    local size = math.min(view.width, view.height)
    graphics2d.beginTarget(view.target, view.camera, Views.clearOptions)
    local cx, cy = view.width * 0.5, view.height * 0.5
    local radius = size * 0.44
    local thickness = size * 0.07
    local start, sweep = math.pi * 0.75, math.pi * 1.5
    local climate = home.climate
    graphics2d.drawArc(cx, cy, radius, thickness, start, start + sweep, '#FFEDEFF4')
    local color = Views.modeColors[climate.mode]
    graphics2d.drawArc(cx, cy, radius, thickness, start, start + sweep * Views.dialShare(climate.target), color)
    local angle = start + sweep * Views.dialShare(climate.current)
    local dotX, dotY = cx + math.cos(angle) * radius, cy + math.sin(angle) * radius
    self:shapeAt(dotX - thickness * 0.7, dotY - thickness * 0.7, thickness * 1.4, thickness * 1.4, 0, '#FFFFFFFF', Views.dotRing)
    for tick = 0, 20 do
        local tickAngle = start + sweep * tick / 20
        local inner, outer = radius - thickness * 1.1, radius - thickness * (tick % 5 == 0 and 1.8 or 1.45)
        graphics2d.drawLine(cx + math.cos(tickAngle) * inner, cy + math.sin(tickAngle) * inner, cx + math.cos(tickAngle) * outer, cy + math.sin(tickAngle) * outer, size * 0.006, Style.color.faint)
    end
    self:text(Views.modeTexts[climate.mode], cx, cy - size * 0.17, size * 0.055, Style.color.muted, 0.5, 0.5, true)
    self:text(Text.format('%.1f°', climate.target, 0.1), cx, cy, size * 0.2, Style.color.ink, 0.5, 0.5, true)
    self:text(Text.format('Indoor %.1f°', climate.current, 0.1), cx, cy + size * 0.17, size * 0.05, Style.color.muted, 0.5, 0.5)
end

-- The share of the dial that a temperature fills, from 10 to 30 degrees.
function Views.dialShare(value)
    return math.max(0, math.min(1, (value - 10) / 20))
end

Views.modeColors = {heat = Style.color.coral, cool = Style.color.sky, auto = Style.color.teal, off = Style.color.faint}
Views.modeTexts = {heat = 'Heating to', cool = 'Cooling to', auto = 'Keeping', off = 'Climate off'}

-- The camera at the front door: the porch and the path in the light of the hour, a tree that sways, clouds that drift, a visitor on the path when the camera sees motion, and the label, the time and the recording dot over the picture.
function Views:drawCamera(view, home)
    local width, height = view.width, view.height
    graphics2d.beginTarget(view.target, view.camera, Views.cameraClear)
    local dark = Views.darkness(home:dayShare())
    local time = home.time
    self:image(Style.skies.day, 0, 0, width, height * 0.6)
    self:image(Style.skies.night, 0, 0, width, height * 0.6, Views.alpha(dark))
    for index = 1, 3 do
        local x = (index * width * 0.37 + time * (10 + index * 4)) % (width * 1.3) - width * 0.15
        local y = height * (0.08 + index * 0.06)
        self:shapeAt(x, y, width * 0.16, height * 0.06, height * 0.03, Views.alpha(0.75 - dark * 0.55), Views.soft)
    end

    -- The lawn, the path to the door and the wall of the house with its door and porch light.
    self:shapeAt(0, height * 0.55, width, height * 0.45, 0, dark > 0.5 and '#FF24402E' or '#FF7BC74D')
    local quad = Views.path
    quad[1][1], quad[1][2] = width * 0.47, height * 0.55
    quad[2][1], quad[2][2] = width * 0.55, height * 0.55
    quad[3][1], quad[3][2] = width * 0.78, height
    quad[4][1], quad[4][2] = width * 0.3, height
    graphics2d.drawPolygon(quad, dark > 0.5 and '#FF5A5A66' or '#FFD9CDB8')
    self:shapeAt(width * 0.36, height * 0.12, width * 0.32, height * 0.45, 0, dark > 0.5 and '#FF8C8478' or '#FFF4EFE6')
    self:shapeAt(width * 0.46, height * 0.24, width * 0.1, height * 0.33, 6, '#FFB07A43')
    local light = dark > 0.3 and '#FFFFD27A' or '#FFCFC6B5'
    self:shapeAt(width * 0.585, height * 0.3, width * 0.022, height * 0.05, 4, light)
    if dark > 0.3 then
        self:shapeAt(width * 0.53, height * 0.22, width * 0.14, height * 0.2, 0, '#FFFFFFFF', Views.glowShape)
    end

    -- The tree beside the house sways in the wind.
    local sway = math.sin(time * 1.3) * width * 0.008
    self:shapeAt(width * 0.17, height * 0.32, width * 0.03, height * 0.3, 6, '#FF6B4A2E')
    self:shapeAt(width * 0.08 + sway, height * 0.1, width * 0.22, height * 0.32, height * 0.16, dark > 0.5 and '#FF2E5A3A' or '#FF4E9E45')

    -- A visitor walks up the path while the camera sees motion, inside the box of the detection.
    if home.motion > 0 then
        local walk = math.min(1, (6 - home.motion) / 4)
        local scale = 1 - walk * 0.45
        local x = width * (0.5 + 0.06 * (1 - walk))
        local feet = height * (0.98 - walk * 0.36)
        local tall = height * 0.42 * scale
        self:shapeAt(x - tall * 0.16, feet - tall * 0.78, tall * 0.32, tall * 0.62, tall * 0.14, '#FF3A4F7A')
        self:shapeAt(x - tall * 0.1, feet - tall, tall * 0.2, tall * 0.2, 0, '#FFE7B48F', Views.ellipse)
        self:shapeAt(x - tall * 0.28, feet - tall * 1.08, tall * 0.56, tall * 1.12, 10, '#00000000', Views.detection)
        self:text('Person', x - tall * 0.28, feet - tall * 1.1, height * 0.045, Style.color.coral, 0, 1, true)
    end

    -- At night the camera sees in infrared, in green, and the lens darkens its corners.
    if dark > 0.5 then
        self:shapeAt(0, 0, width, height, 0, '#3322C58A')
    end
    self:shapeAt(-width * 0.1, -height * 0.1, width * 1.2, height * 1.2, 0, '#FFFFFFFF', Views.lensShape)
    self:shapeAt(width * 0.03, height * 0.05, width * 0.27, height * 0.1, height * 0.05, '#88000000')
    if time % 1.2 < 0.7 then
        self:shapeAt(width * 0.045, height * 0.075, height * 0.045, height * 0.045, 0, Style.color.red, Views.ellipse)
    end
    self:text('Front door', width * 0.045 + height * 0.06, height * 0.1, height * 0.05, '#FFFFFFFF', 0, 0.5, true)
    self:text(Text.clock(home.minute * 60), width * 0.96, height * 0.1, height * 0.05, '#FFFFFFFF', 1, 0.5, true)
end

Views.path = {{0, 0}, {0, 0}, {0, 0}, {0, 0}}

-- The energy of the day: the sun on the roof as an amber area over the hours, what the home used so far as a coral line, and the hour that runs now.
function Views:drawEnergy(view, home)
    local width, height = view.width, view.height
    graphics2d.beginTarget(view.target, view.camera, Views.clearOptions)
    local left, right = width * 0.02, width * 0.98
    local top, bottom = height * 0.08, height * 0.82
    local peak = 6.5
    for line = 0, 3 do
        local y = bottom - (bottom - top) * line / 3
        graphics2d.drawLine(left, y, right, y, 2, Style.color.line)
    end

    local curve = self.curve
    for slot = 0, 96 do
        local value = home.solarCurve[math.min(slot + 1, 96)]
        local x = left + (right - left) * slot / 96
        local y = bottom - (bottom - top) * value / peak
        local upper, lower = curve[slot * 2 + 1], curve[slot * 2 + 2]
        upper.x, upper.y, upper.u, upper.v = x, y, 0.5, (y - top) / (bottom - top)
        lower.x, lower.y, lower.u, lower.v = x, bottom, 0.5, 1
    end
    graphics2d.drawMesh(Style.solarFill, curve, self.indices)

    -- The usage runs from midnight to the hour of now.
    local now = math.floor((home.minute % (24 * 60)) / 15) + 1
    local line = self.usageLine
    for slot = 1, now do
        line[slot * 2 - 1] = left + (right - left) * (slot - 0.5) / 96
        line[slot * 2] = bottom - (bottom - top) * math.min(home.usage[slot], peak) / peak
    end
    if now >= 2 then
        self.usageOptions.count = now
        graphics2d.drawStroke(line, self.usageOptions)
    end
    local nowX = left + (right - left) * home:dayShare()
    graphics2d.drawLine(nowX, top, nowX, bottom, 3, Style.color.ink)
    self:shapeAt(nowX - 7, top - 7, 14, 14, 0, Style.color.ink, Views.ellipse)
    for hour = 0, 24, 6 do
        self:text(Views.hours[hour], left + (right - left) * hour / 24, height * 0.92, height * 0.07, Style.color.muted, hour == 0 and 0 or (hour == 24 and 1 or 0.5), 0.5)
    end
end

Views.hours = {[0] = '0:00', [6] = '6:00', [12] = '12:00', [18] = '18:00', [24] = '24:00'}
Views.clearOptions = {clear = '#00000000'}
Views.cameraClear = {clear = '#FF1D2433'}
Views.ellipse = {kind = 'ellipse'}
Views.soft = {softness = 18}
Views.dotRing = {kind = 'ellipse', borderWidth = 6, borderColor = Style.color.ink}
Views.detection = {borderWidth = 4, borderColor = Style.color.coral}
Views.glowShape = {kind = 'ellipse'}
Views.lensShape = {kind = 'ellipse'}

-- The textures of the shapes exist once the style loaded them.
function Views.prepare()
    Views.glowShape.texture = Style.glow
    Views.lensShape.texture = Style.lens
end

return Views
