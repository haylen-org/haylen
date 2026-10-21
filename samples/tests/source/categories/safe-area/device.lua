-- A phone, a tablet or a TV drawn with shapes, which the orientation and safe area tests hold their screens in: a body with a rim and a soft shadow, a screen that a color or a picture fills, the dynamic island, the notch or the camera of its kind, its side buttons and its home indicator. The size and the parts are those of the device held upright, and an angle turns the whole device around its center, so a phone turns into landscape with its island on the left.
local graphics2d = require('haylen.graphics2d')

local Device = {}

-- The parts of each kind, as shares of the width of the device held upright: the corner radius, the bezel around the screen and the cutout at the top of the screen.
Device.kinds = {
    island = {radius = 0.15, bezel = 0.045, cutout = 'island', buttons = true, indicator = true},
    notch = {radius = 0.15, bezel = 0.045, cutout = 'notch', buttons = true, indicator = true},
    android = {radius = 0.1, bezel = 0.04, cutout = 'hole', buttons = true, indicator = true},
    tablet = {radius = 0.06, bezel = 0.05, cutout = 'camera', indicator = true},
    tv = {radius = 0.012, bezel = 0.018, stand = true},
}
Device.colors = {body = '#FF2B2F40', rim = '#FF4A5170', outline = '#FF15172A', cutout = '#FF07080C', indicator = '#E6FFFFFF', shadow = '#70000000'}

-- Returns the world point of a point of the device held upright, `x` across and `y` down from its center, turned by the angle.
function Device.place(centerX, centerY, angle, x, y)
    local cos, sin = math.cos(angle), math.sin(angle)
    return centerX + x * cos - y * sin, centerY + x * sin + y * cos
end

-- Draws a rounded part of the device: a box of `width` by `height` centered at `x`, `y` of the device held upright, turned with it.
function Device.part(options, x, y, width, height, shape)
    local px, py = Device.place(options.x, options.y, options.angle or 0, x, y)
    shape.rotation = options.angle or 0
    shape.layer = (options.layer or 0) + (shape.layer or 0)
    graphics2d.drawShape({px - width / 2, py - height / 2, width, height}, shape)
end

-- Returns the size of the screen of a device of the width and height held upright.
function Device.screenSize(kind, width, height)
    local bezel = width * Device.kinds[kind].bezel
    return width - bezel * 2, height - bezel * 2
end

-- Returns the options of a device of the kind whose screen covers a rectangle, held upright or turned a quarter turn into landscape with `sideways`, and the corner radius of that screen.
function Device.around(kind, rect, sideways)
    local share = Device.kinds[kind]
    local screenWidth, screenHeight = rect[3], rect[4]
    if sideways then
        screenWidth, screenHeight = screenHeight, screenWidth
    end
    local width = screenWidth / (1 - share.bezel * 2)
    local bezel = width * share.bezel
    local options = {kind = kind, x = rect[1] + rect[3] / 2, y = rect[2] + rect[4] / 2, width = width, height = screenHeight + bezel * 2, angle = sideways and -math.pi / 2 or 0}
    return options, math.max(0, width * share.radius - bezel)
end

-- Draws a device from a table with `kind`, the center `x` and `y`, the `width` and `height` held upright, the `angle` it turns by, the `screen` color, an optional `texture` stretched over the screen and the `layer` it starts at.
function Device.draw(options)
    local kind = Device.kinds[options.kind or 'island']
    local width, height = options.width, options.height
    local radius = width * kind.radius
    local bezel = width * kind.bezel
    local colors = Device.colors
    local outline = math.max(3, width * 0.012)

    if kind.stand then
        Device.part(options, 0, height * 0.5 + width * 0.03, width * 0.08, width * 0.06, {color = colors.body, borderWidth = outline, borderColor = colors.outline, layer = -1})
        Device.part(options, 0, height * 0.5 + width * 0.065, width * 0.3, width * 0.022, {radius = width * 0.011, color = colors.body, borderWidth = outline, borderColor = colors.outline, layer = -1})
    end
    if kind.buttons then
        Device.part(options, width * 0.5 + outline * 0.5, -height * 0.18, outline * 2.4, height * 0.12, {radius = outline, color = colors.rim, borderWidth = outline * 0.6, borderColor = colors.outline, layer = -1})
        Device.part(options, -width * 0.5 - outline * 0.5, -height * 0.24, outline * 2.4, height * 0.07, {radius = outline, color = colors.rim, borderWidth = outline * 0.6, borderColor = colors.outline, layer = -1})
        Device.part(options, -width * 0.5 - outline * 0.5, -height * 0.13, outline * 2.4, height * 0.07, {radius = outline, color = colors.rim, borderWidth = outline * 0.6, borderColor = colors.outline, layer = -1})
    end
    local shadowX, shadowY = Device.place(options.x, options.y, options.angle or 0, 0, 0)
    graphics2d.drawShape({shadowX - width / 2, shadowY - height / 2 + width * 0.05, width, height}, {radius = radius, rotation = options.angle or 0, color = colors.shadow, softness = width * 0.12, layer = (options.layer or 0) - 2})
    Device.part(options, 0, 0, width, height, {radius = radius, color = colors.body, borderWidth = outline, borderColor = colors.outline})
    Device.part(options, 0, 0, width - outline * 2.5, height - outline * 2.5, {radius = radius - outline, color = '#00000000', borderWidth = math.max(1.5, outline * 0.5), borderColor = colors.rim, layer = 1})

    local screenWidth, screenHeight = width - bezel * 2, height - bezel * 2
    Device.part(options, 0, 0, screenWidth, screenHeight, {radius = math.max(0, radius - bezel), color = options.screen or '#FF2F86F6', texture = options.texture, layer = 2})

    -- The cutout and the indicator sit on the screen, at its top and bottom as the device is held upright.
    local top = -screenHeight / 2
    if kind.cutout == 'island' then
        Device.part(options, 0, top + width * 0.055, width * 0.3, width * 0.085, {radius = width * 0.0425, color = colors.cutout, layer = 4})
    elseif kind.cutout == 'notch' then
        Device.part(options, 0, top + width * 0.035, width * 0.46, width * 0.07, {radius = {0, 0, width * 0.035, width * 0.035}, color = colors.cutout, layer = 4})
    elseif kind.cutout == 'hole' then
        Device.part(options, 0, top + width * 0.05, width * 0.05, width * 0.05, {kind = 'ellipse', color = colors.cutout, layer = 4})
    elseif kind.cutout == 'camera' then
        Device.part(options, 0, -height / 2 + bezel / 2, bezel * 0.35, bezel * 0.35, {kind = 'ellipse', color = colors.cutout, layer = 4})
    end
    if kind.indicator then
        Device.part(options, 0, screenHeight / 2 - width * 0.035, width * 0.34, width * 0.016, {radius = width * 0.008, color = options.indicator or colors.indicator, layer = 4})
    end
end

return Device
