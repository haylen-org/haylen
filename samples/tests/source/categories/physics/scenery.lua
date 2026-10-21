-- The scenery of the physics tests, painted in the style of the project: a day sky with drifting clouds, misty mountains and green hills behind the stage, a grassy ground strip whose surface is where bodies rest, terrain of any outline with grass along its top, and water. Every layer reaches the edges of what the camera shows, so a stage never shows the void around it at any size of the play area.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Scenery = {}

Scenery.dirt = '#FF80401E'
Scenery.skyTop = '#FF63BDEB'
Scenery.skyBottom = '#FFD4F1FB'

-- The ground strip draws at this many world units per pixel of its image, with its grass surface this many pixels below its top.
Scenery.groundScale = 0.6
Scenery.groundSurface = 54

-- The clouds of the strip one by one, by the area of each in its image, placed above the ground in a pattern that repeats every period.
Scenery.cloudPeriod = 2600
Scenery.clouds = {
    {source = {92, 95, 214, 139}, x = 0, y = -620, top = 30, scale = 1.1},
    {source = {742, 124, 153, 103}, x = 700, y = -700, top = 14, scale = 0.85},
    {source = {467, 71, 263, 172}, x = 1250, y = -650, top = 24, scale = 1.05},
    {source = {320, 119, 135, 98}, x = 2000, y = -560, top = 50, scale = 0.8},
}

local textures = {}

local function texture(name, path, options)
    if not textures[name] then
        textures[name] = assets.texture(path, options or {filter = 'linear', wrap = 'repeat'})
    end
    return textures[name]
end

local function gradient(name, stops)
    if not textures[name] then
        local image = graphics.newImage(4, 256)
        image:fillGradient({angle = math.pi / 2, stops = stops})
        textures[name] = graphics.newTexture(image, {filter = 'linear'})
    end
    return textures[name]
end

-- A band of rows of the ground strip, which strokes along terrain of any outline: the grass at its top or the layers of dirt under it. The band takes `padding` clear rows under it, so the clear top of the grass never takes the color of its bottom where the texture repeats.
local function band(name, top, height, padding)
    if not textures[name] then
        local image = graphics.newImage(1076, height + padding)
        image:blit(assets.load('camera/ground.png', 'image'), 0, 0, {source = {0, top, 1076, height}})
        image:bleed()
        textures[name] = graphics.newTexture(image, {filter = 'linear', wrap = 'repeat'})
    end
    return textures[name]
end

local area = {0, 0, 0, 0}
local fillOrder = {layer = 0}
local cloudOptions = {source = {0, 0, 0, 0}, width = 0, height = 0, pivotX = 0, pivotY = 0, color = '#F2FFFFFF', layer = 0}
local options = {width = 0, height = 0, pivotX = 0, pivotY = 0, color = '#FFFFFFFF', layer = 0}

local function drawImage(image, x, y, width, height, color, layer)
    options.width, options.height, options.color, options.layer = width, height, color or '#FFFFFFFF', layer
    graphics2d.draw(image, x, y, options)
end

-- Repeats an image along the whole width the camera shows, from `x` with a scroll that moves it as far layers move.
local function drawRow(image, y, width, height, scroll, color, layer)
    local bounds = graphics2d.canvasBounds()
    local first = math.floor((bounds.x - scroll) / width) * width + scroll
    for x = first, bounds.x + bounds.width, width do
        drawImage(image, x, y, width, height, color, layer)
    end
end

-- Draws the day sky, the clouds, the mountains and the hills behind a stage whose ground lies at `groundY`. The table `settings` takes `hills = false` for a stage without them, `parallax`, how far the far layers follow the camera from 0 to 1, and the `layer` of the sky, with every other layer above it.
function Scenery.sky(camera, groundY, settings)
    settings = settings or {}
    local layer = settings.layer or -900
    local bounds = graphics2d.canvasBounds()
    local sky = gradient('sky', {{0, Scenery.skyTop}, {1, Scenery.skyBottom}})
    local top = math.min(bounds.y, groundY - 800)
    drawImage(sky, bounds.x, top, bounds.width, groundY - top, nil, layer)
    area[1], area[2], area[3], area[4] = bounds.x, groundY, bounds.width, math.max(0, bounds.y + bounds.height - groundY)
    fillOrder.layer = layer
    graphics2d.drawRect(area, Scenery.skyBottom, fillOrder)

    local parallax = settings.parallax or 0
    local x = camera.position.x * parallax
    local clouds = texture('clouds', 'camera/clouds.png')
    local drift = x * 0.92 + haylen.elapsed() * 12
    local first = math.floor((bounds.x - drift - 400) / Scenery.cloudPeriod) * Scenery.cloudPeriod + drift
    for start = first, bounds.x + bounds.width, Scenery.cloudPeriod do
        for _, cloud in ipairs(Scenery.clouds) do
            local source = cloud.source
            cloudOptions.source, cloudOptions.width, cloudOptions.height, cloudOptions.layer = source, source[3] * cloud.scale, source[4] * cloud.scale, layer + 1
            graphics2d.draw(clouds, start + cloud.x, math.max(groundY + cloud.y, bounds.y + cloud.top), cloudOptions)
        end
    end
    drawRow(texture('mountains', 'camera/mountains.png'), groundY - 380, 820, 367, x * 0.85, '#80FFFFFF', layer + 2)
    if settings.hills ~= false then
        drawRow(texture('hills', 'camera/hills.png'), groundY - 300, 760, 373, x * 0.65, '#FFFFFFFF', layer + 3)
    end
end

-- Draws the grassy ground strip with its surface at `y`, from `left` to `right`, or across all the camera shows, and the dirt below it down to the bottom of the view.
function Scenery.ground(y, left, right, layer)
    layer = layer or -10
    local bounds = graphics2d.canvasBounds()
    left = math.max(left or bounds.x, bounds.x - 600)
    right = math.min(right or (bounds.x + bounds.width), bounds.x + bounds.width + 600)
    local scale = Scenery.groundScale
    local width, height = 1076 * scale, 256 * scale
    local top = y - Scenery.groundSurface * scale
    local image = texture('ground', 'camera/ground.png')
    local first = math.floor(left / width) * width
    graphics2d.pushClip({left, bounds.y, right - left, bounds.height})
    for x = first, right, width do
        drawImage(image, x, top, width, height, nil, layer)
    end
    graphics2d.popClip()
    area[1], area[2], area[3], area[4] = left, top + height - 1, right - left, math.max(0, bounds.y + bounds.height - top - height + 1)
    fillOrder.layer = layer
    graphics2d.drawRect(area, Scenery.dirt, fillOrder)
end

-- A stroke along a surface runs from right to left, so the top of a band of the ground strip faces up.
local function along(points, offset)
    local flat = {}
    for index = #points, 1, -1 do
        flat[#flat + 1] = points[index][1]
        flat[#flat + 1] = points[index][2] + offset
    end
    return flat
end

-- Returns the painted grass along `points`, a list of `{x, y}` from left to right, which `Scenery.drawGrass` draws on top of ground of any material.
function Scenery.newGrass(points)
    local scale = Scenery.groundScale
    return {surface = along(points, (126 / 2 - Scenery.groundSurface) * scale), stroke = {width = 126 * scale, texture = band('grass', 0, 124, 2), textureMode = 'tile', textureLength = 1076 * scale, join = 'round', layer = 0}}
end

function Scenery.drawGrass(grass, layer)
    grass.stroke.layer = layer or -9
    graphics2d.drawStroke(grass.surface, grass.stroke)
end

-- Returns terrain whose surface follows `points`, a list of `{x, y}` from left to right, which `Scenery.drawTerrain` draws: the layers of dirt of the ground strip along the surface, the deep dirt down to `bottom`, far below the surface by default, and the painted grass along the top.
function Scenery.newTerrain(points, bottom)
    local scale = Scenery.groundScale
    local lowest = -math.huge
    local fill = {}
    for index, point in ipairs(points) do
        fill[index] = {point[1], point[2]}
        lowest = math.max(lowest, point[2])
    end
    bottom = bottom or lowest + 4000
    fill[#fill + 1] = {points[#points][1], bottom}
    fill[#fill + 1] = {points[1][1], bottom}
    local dirt = {surface = along(points, (112 + 72 - Scenery.groundSurface) * scale), stroke = {width = 144 * scale, texture = band('dirt', 112, 144, 0), textureMode = 'tile', textureLength = 1076 * scale, join = 'round', layer = 0}}
    return {fill = fill, dirt = dirt, grass = Scenery.newGrass(points)}
end

function Scenery.drawTerrain(terrain, layer)
    layer = layer or -10
    fillOrder.layer = layer
    graphics2d.drawPolygon(terrain.fill, Scenery.dirt, fillOrder)
    terrain.dirt.stroke.layer = layer
    graphics2d.drawStroke(terrain.dirt.surface, terrain.dirt.stroke)
    Scenery.drawGrass(terrain.grass, layer + 1)
end

local wave = {}
local surf = {0.75, 0.95, 1, 1}
local foam = {width = 1, color = '#CCFFFFFF', join = 'round', cap = 'round', count = 2, layer = 0}

-- Draws water from `left` to `right` whose surface lies at `y`, with waves that roll along it and the deep water down to the bottom of the view. The table `settings` takes the `layer`, which is in front of the scenery by default so bodies sink behind its surface, and `alpha` for water that shows what is in it.
function Scenery.water(left, right, y, settings)
    settings = settings or {}
    local layer = settings.layer or 20
    local bounds = graphics2d.canvasBounds()
    local bottom = bounds.y + bounds.height + 40
    local alpha = settings.alpha or 0.85
    local deep = gradient('water', {{0, '#FF38C2D2'}, {1, '#FF1C6E9A'}})
    drawImage(deep, left, y, right - left, bottom - y, {1, 1, 1, alpha}, layer)
    local time = haylen.elapsed()
    local step = 24
    local count = 0
    for x = left, right + step - 1, step do
        local px = math.min(x, right)
        count = count + 1
        wave[count * 2 - 1] = px
        wave[count * 2] = y + math.sin(px * 0.02 + time * 2.2) * 4 + math.sin(px * 0.047 - time * 1.3) * 2.5
    end
    local unit = graphics2d.canvasUnitSize()
    foam.count, foam.layer = count, layer + 1
    surf[4] = alpha
    foam.width, foam.color = 8 * unit, surf
    graphics2d.drawStroke(wave, foam)
    foam.width, foam.color = 3 * unit, '#E6FFFFFF'
    graphics2d.drawStroke(wave, foam)
end

return Scenery
