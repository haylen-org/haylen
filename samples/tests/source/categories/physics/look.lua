-- The look of the bodies of the physics tests, in the art style of the project: every shape of a body in a material, with a bold navy outline, two tones lit from the top left, one soft highlight and the details of the material, such as the planks of a crate or the rivets of steel, which turn with the body. A body takes its look from its shapes once, or from an outline the test gives for a concave polygon or the solid side of a chain, and draws without making a table.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local Look = {}

Look.ink = '#FF1E1B33'
Look.outlinePixels = 3.5

-- World units of one pixel of the screen, which the test sets from the zoom of its camera before it draws, so outlines keep their width at any zoom.
Look.pixel = 1

local function tones(fill, shade, light, detail, extra)
    local material = {fill = fill, shade = shade, light = light, detail = detail}
    for key, value in pairs(extra or {}) do
        material[key] = value
    end
    return material
end

-- Every material has a fill, the shade of the side away from the light, a highlight and the detail it draws on its shapes.
Look.materials = {
    wood = tones('#FFE9A65B', '#FFB8743A', '#FFFFD9A3', 'planks', {grain = '#C08E4A22'}),
    crate = tones('#FFE9A65B', '#FFB8743A', '#FFFFD9A3', 'crate', {panel = '#FFD18A45', grain = '#C08E4A22'}),
    darkWood = tones('#FFA86B3C', '#FF7A4724', '#FFD39A62', 'planks', {grain = '#C05A3418'}),
    stone = tones('#FFB9B5D0', '#FF8D88AD', '#FFE6E4F4', 'cracks'),
    slate = tones('#FF7C82A6', '#FF585E80', '#FFA6ACCC', 'cracks'),
    brick = tones('#FFE57A55', '#FFB4553A', '#FFFFAE8E', 'bevel'),
    steel = tones('#FFA9BCD3', '#FF7088A6', '#FFE9F2FB', 'rivets'),
    iron = tones('#FF66708A', '#FF474F66', '#FF9AA5BE', 'rivets'),
    gold = tones('#FFFFC93C', '#FFE09A1E', '#FFFFF2A8', 'bevel'),
    ice = tones('#EECDF3FF', '#EE8FD3EE', '#FFFFFFFF', 'streaks'),
    glass = tones('#668FE0F5', '#995FB8D8', '#EEFFFFFF', 'streaks'),
    rubber = tones('#FF3C3F52', '#FF26283A', '#FF6E7390', nil),
    skin = tones('#FFFFCDA2', '#FFE09A72', '#FFFFE8D2', nil),
    tire = tones('#FF3A3D4E', '#FF24263A', '#FF6A6F88', nil, {hub = '#FFC9D3E2', hubShade = '#FF8090A8'}),
}

-- Materials painted as pictures, which boxes about as wide as tall and round bodies draw stretched over their shape, and the share of the picture its painted object covers.
Look.pictures = {
    woodCrate = {path = 'physics/crate.png', cover = {152 / 160, 143 / 151}, fallback = 'crate'},
    steelCrate = {path = 'physics/steel-crate.png', cover = {152 / 160, 145 / 153}, fallback = 'steel'},
    barrel = {path = 'physics/barrel.png', cover = {156 / 164, 184 / 192}, fallback = 'brick'},
    wheel = {path = 'physics/wheel.png', cover = {149 / 157, 152 / 160}, fallback = 'tire'},
}

-- Painted toy colors, for bodies whose color tells them apart, such as balls of a team or blocks of a stack.
Look.toys = {
    tones('#FFF0473E', '#FFB9302A', '#FFFF9A92', 'bevel'),
    tones('#FF2F86F6', '#FF1E5FB8', '#FF96C6FF', 'bevel'),
    tones('#FFFFC93C', '#FFD99A12', '#FFFFF0A0', 'bevel'),
    tones('#FF7BC74D', '#FF4E9A32', '#FFC2EE96', 'bevel'),
    tones('#FF9A6BE0', '#FF6E44B0', '#FFD6BEFF', 'bevel'),
    tones('#FF2FB3C4', '#FF1E8494', '#FF9BE9F2', 'bevel'),
    tones('#FFFF9A3C', '#FFD06E1E', '#FFFFD09A', 'bevel'),
    tones('#FFFF7FB0', '#FFD0507E', '#FFFFC6DC', 'bevel'),
}
Look.toyNames = {red = 1, blue = 2, yellow = 3, green = 4, purple = 5, teal = 6, orange = 7, pink = 8}

-- Reused for every draw, so drawing a body makes no table.
local rect = {0, 0, 0, 0}
local shape = {kind = 'rectangle', radius = 0, rotation = 0, color = '#FFFFFFFF', borderWidth = 0, borderColor = '#FF1E1B33', layer = 0}
local order = {layer = 0}
local line = {0, 0, 0, 0}
local stroke = {width = 1, color = '#FFFFFFFF', cap = 'round', join = 'round', closed = false, layer = 0}

-- Returns the material of a name, a toy color, a picture or a material table. A picture keeps the material it falls back to for shapes it does not fit.
function Look.material(material)
    if type(material) == 'table' then
        return material
    end
    if Look.toyNames[material] then
        return Look.toys[Look.toyNames[material]]
    end
    local picture = Look.pictures[material]
    if picture then
        if not picture.texture then
            picture.texture = assets.texture(picture.path, {filter = 'linear'})
            picture.material = Look.material(picture.fallback)
        end
        return picture
    end
    return Look.materials[material] or error(string.format('The physics look has no material "%s".', tostring(material)))
end

-- Returns a copy of a material in other colors, such as a blue steel.
function Look.tint(material, fill, shade, light)
    local copy = {}
    for key, value in pairs(Look.material(material)) do
        copy[key] = value
    end
    copy.fill, copy.shade, copy.light = fill, shade or copy.shade, light or copy.light
    return copy
end

local function signedArea(points)
    local area = 0
    for index = 1, #points do
        local a, b = points[index], points[index % #points + 1]
        area = area + a[1] * b[2] - b[1] * a[2]
    end
    return area / 2
end

-- Moves every edge of a simple polygon inward by `distance`, with miters that stop at three times the distance at sharp corners.
local function inset(points, distance)
    local count = #points
    local inward = signedArea(points) > 0 and 1 or -1
    local result = {}
    for index = 1, count do
        local previous, point, following = points[(index - 2) % count + 1], points[index], points[index % count + 1]
        local ax, ay = point[1] - previous[1], point[2] - previous[2]
        local bx, by = following[1] - point[1], following[2] - point[2]
        local la, lb = math.max(math.sqrt(ax * ax + ay * ay), 1e-6), math.max(math.sqrt(bx * bx + by * by), 1e-6)
        local n1x, n1y = -ay / la * inward, ax / la * inward
        local n2x, n2y = -by / lb * inward, bx / lb * inward
        local mx, my = n1x + n2x, n1y + n2y
        local scale = distance / math.max(1 + n1x * n2x + n1y * n2y, 1e-6)
        local length = math.sqrt(mx * mx + my * my) * scale
        if length > distance * 3 then
            scale = scale * distance * 3 / length
        end
        result[index] = {point[1] + mx * scale, point[2] + my * scale}
    end
    return result
end

local function isBox(points)
    if #points ~= 4 then
        return false
    end
    local a, b, c, d = points[1], points[2], points[3], points[4]
    local ex, ey = b[1] - a[1], b[2] - a[2]
    local fx, fy = c[1] - b[1], c[2] - b[2]
    local gx, gy = d[1] - c[1], d[2] - c[2]
    local dot = ex * fx + ey * fy
    local length = math.sqrt(ex * ex + ey * ey) * math.sqrt(fx * fx + fy * fy)
    return math.abs(dot) < length * 1e-3 and math.abs(ex + gx) < 1e-3 and math.abs(ey + gy) < 1e-3
end

-- A small hash of the size of a part, so the cracks and streaks of a material sit differently on every part without randomness.
local function hash(a, b)
    local value = math.sin(a * 12.9898 + b * 78.233) * 43758.5453
    return value - math.floor(value)
end

local function boxPart(points)
    local a, b, c = points[1], points[2], points[3]
    local width = math.sqrt((b[1] - a[1]) ^ 2 + (b[2] - a[2]) ^ 2)
    local height = math.sqrt((c[1] - b[1]) ^ 2 + (c[2] - b[2]) ^ 2)
    local part = {kind = 'box', x = (a[1] + c[1]) / 2, y = (a[2] + c[2]) / 2, angle = math.atan(b[2] - a[2], b[1] - a[1]), width = width, height = height}
    part.size = math.min(width, height)
    return part
end

local function polygonPart(points)
    local copy, minX, minY, maxX, maxY = {}, math.huge, math.huge, -math.huge, -math.huge
    for index, point in ipairs(points) do
        local x, y = point[1] or point.x, point[2] or point.y
        copy[index] = {x, y}
        minX, minY, maxX, maxY = math.min(minX, x), math.min(minY, y), math.max(maxX, x), math.max(maxY, y)
    end
    local part = {kind = 'polygon', points = copy, size = math.min(maxX - minX, maxY - minY), world = {}, lit = {}, flat = {}}
    for index = 1, #copy do
        part.world[index] = {0, 0}
        part.lit[index] = {0, 0}
    end
    return part
end

-- Turns the shapes of a body, or the outlines of the options, into the parts its look draws, in the space of the body.
local function partsOf(body, options)
    local list = {}
    if options.outline then
        list[1] = polygonPart(options.outline)
        return list
    end
    local chain
    for _, shape in ipairs(body:shapes()) do
        local kind, points = shape.kind, shape.points
        if not shape.sensor then
            if kind == 'circle' then
                list[#list + 1] = {kind = 'circle', x = points[1].x, y = points[1].y, radius = shape.radius, size = shape.radius * 2}
            elseif kind == 'capsule' then
                local a, b = points[1], points[2]
                local length = math.sqrt((b.x - a.x) ^ 2 + (b.y - a.y) ^ 2)
                list[#list + 1] = {kind = 'capsule', x = (a.x + b.x) / 2, y = (a.y + b.y) / 2, angle = math.atan(b.y - a.y, b.x - a.x), width = length + shape.radius * 2, height = shape.radius * 2, size = shape.radius * 2}
            elseif kind == 'polygon' then
                local corners = {}
                for index, point in ipairs(points) do
                    corners[index] = {point.x, point.y}
                end
                list[#list + 1] = isBox(corners) and boxPart(corners) or polygonPart(corners)
            elseif kind == 'segment' or kind == 'chainSegment' then
                if chain and chain.last[1] == points[1].x and chain.last[2] == points[1].y then
                    chain.points[#chain.points + 1] = points[2].x
                    chain.points[#chain.points + 1] = points[2].y
                else
                    chain = {kind = 'line', points = {points[1].x, points[1].y, points[2].x, points[2].y}, size = options.thickness or 8}
                    list[#list + 1] = chain
                end
                chain.last = {points[2].x, points[2].y}
            end
        end
    end
    for _, part in ipairs(list) do
        if part.kind == 'line' then
            part.world = {}
        end
    end
    return list
end

-- Dresses a body in a material, a name of `Look.materials`, a toy color such as `'blue'` or a material table. The table `options` takes an `outline` that replaces the shapes, a `radius` that rounds the corners of boxes, `spin = true`, which marks round bodies with spots that show them roll, and the `thickness` of segments and chains.
function Look.dress(body, material, options)
    options = options or {}
    local look = {material = Look.material(material), parts = partsOf(body, options), radius = options.radius or 3, spin = options.spin, thickness = options.thickness or 8}
    if type(body.data) ~= 'table' then
        body.data = {}
    end
    body.data.look = look
    return body
end

local function outlineWidth(size)
    return math.min(Look.outlinePixels * Look.pixel, size * 0.12)
end

local function drawShapeAt(kind, x, y, width, height, radius, rotation, color, border, layer)
    rect[1], rect[2], rect[3], rect[4] = x - width / 2, y - height / 2, width, height
    shape.kind, shape.radius, shape.rotation, shape.color = kind, radius, rotation, color
    shape.borderWidth, shape.layer = border, layer
    graphics2d.drawShape(rect, shape)
end

local function drawLine(x1, y1, x2, y2, width, color, layer)
    line[1], line[2], line[3], line[4] = x1, y1, x2, y2
    stroke.width, stroke.color, stroke.closed, stroke.layer = width, color, false, layer
    graphics2d.drawStroke(line, stroke)
end

local function drawDot(x, y, radius, color, layer)
    order.layer = layer
    graphics2d.drawCircle(x, y, radius, color, order)
end

-- The highlight lies along the edge that faces up, near its end toward the light, so it stays on top as the body turns.
local function boxHighlight(x, y, width, height, angle, k, color, layer)
    local best, bestY = 0, math.huge
    for side = 0, 3 do
        local a = angle + side * math.pi / 2
        local ny = math.sin(a - math.pi / 2)
        if ny < bestY then
            best, bestY = side, ny
        end
    end
    local a = angle + best * math.pi / 2
    local along = best % 2 == 0 and width or height
    local across = best % 2 == 0 and height or width
    local cos, sin = math.cos(a), math.sin(a)
    local nx, ny = math.cos(a - math.pi / 2), math.sin(a - math.pi / 2)
    local thickness = math.max(2 * Look.pixel, math.min(across * 0.09, 6))
    local length = along * 0.34
    local offset = across / 2 - k - thickness * 0.9
    local start = -along / 2 + k + thickness
    if cos < 0 then
        start = along / 2 - k - thickness - length
    end
    local cx = x + nx * offset + cos * (start + length / 2)
    local cy = y + ny * offset + sin * (start + length / 2)
    if length > thickness * 1.5 and offset > 0 then
        drawShapeAt('rectangle', cx, cy, length, thickness, thickness / 2, a, color, 0, layer)
    end
end

local function point(part, px, py, bx, by, cos, sin)
    return bx + (part.x + px) * cos - (part.y + py) * sin, by + (part.x + px) * sin + (part.y + py) * cos
end

local function boxPoint(part, u, v, bx, by, cos, sin)
    local ca, sa = math.cos(part.angle), math.sin(part.angle)
    return point(part, u * ca - v * sa, u * sa + v * ca, bx, by, cos, sin)
end

-- The details of a material on a box, in the space of the box, so they turn with it.
local function boxDetail(part, material, bx, by, cos, sin, angle, ow, layer)
    local detail = material.detail
    local w, h = part.width, part.height
    local size = part.size
    if detail == 'crate' and size >= 24 then
        local frame = math.max(size * 0.16, ow * 1.2)
        local x, y = boxPoint(part, 0, 0, bx, by, cos, sin)
        local iw, ih = w - frame * 2, h - frame * 2
        shape.borderColor = material.shade
        drawShapeAt('rectangle', x, y, iw, ih, 1, angle, material.panel, math.max(1.2, ow * 0.45), layer)
        shape.borderColor = Look.ink
        local diagonal = math.sqrt(iw * iw + ih * ih)
        local brace = math.atan(-ih, iw)
        drawShapeAt('rectangle', x, y, diagonal - frame * 0.6, frame * 0.85, 1, angle + brace, material.fill, 0, layer)
        local nail = math.max(1.2, size * 0.035)
        for _, corner in ipairs({{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}) do
            local nx, ny = boxPoint(part, corner[1] * (w / 2 - frame / 2), corner[2] * (h / 2 - frame / 2), bx, by, cos, sin)
            drawDot(nx, ny, nail, material.shade, layer)
        end
    elseif detail == 'planks' then
        local long, short = math.max(w, h), math.min(w, h)
        local alongX = w >= h
        if short >= 12 then
            for index, share in ipairs({-0.2, 0.22}) do
                local from, to = -long / 2 + long * (0.12 + 0.18 * index), long / 2 - long * (0.38 - 0.16 * index)
                local u1, v1, u2, v2 = from, share * short, to, share * short
                if not alongX then
                    u1, v1, u2, v2 = v1, u1, v2, u2
                end
                local x1, y1 = boxPoint(part, u1, v1, bx, by, cos, sin)
                local x2, y2 = boxPoint(part, u2, v2, bx, by, cos, sin)
                drawLine(x1, y1, x2, y2, math.max(1, short * 0.06), material.shade, layer)
            end
        end
        if long >= 60 and short >= 10 then
            local nail = math.max(1.2, short * 0.07)
            for _, side in ipairs({-1, 1}) do
                local u, v = side * (long / 2 - short * 0.45), 0
                if not alongX then
                    u, v = v, u
                end
                local x, y = boxPoint(part, u, v, bx, by, cos, sin)
                drawDot(x, y, nail, material.shade, layer)
            end
        end
    elseif detail == 'rivets' and size >= 20 then
        local rivet = math.max(1.5, size * 0.05)
        local inset = ow + rivet * 2.2
        for _, corner in ipairs({{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}) do
            local x, y = boxPoint(part, corner[1] * (w / 2 - inset), corner[2] * (h / 2 - inset), bx, by, cos, sin)
            drawDot(x, y, rivet, material.shade, layer)
            drawDot(x - rivet * 0.3, y - rivet * 0.3, rivet * 0.45, material.light, layer)
        end
    elseif detail == 'cracks' and size >= 24 then
        local seed = hash(w, h)
        local u = (seed - 0.5) * w * 0.4
        local x1, y1 = boxPoint(part, u, -h / 2 + ow, bx, by, cos, sin)
        local x2, y2 = boxPoint(part, u + w * 0.08, -h / 2 + h * 0.28, bx, by, cos, sin)
        local x3, y3 = boxPoint(part, u - w * 0.04, -h / 2 + h * 0.42, bx, by, cos, sin)
        local width = math.max(1.2, size * 0.035)
        drawLine(x1, y1, x2, y2, width, material.shade, layer)
        drawLine(x2, y2, x3, y3, width, material.shade, layer)
        local x4, y4 = boxPoint(part, w / 2 - ow - w * 0.18, h / 2 - ow - h * 0.2, bx, by, cos, sin)
        drawDot(x4, y4, math.max(1.5, size * 0.05), material.shade, layer)
    elseif detail == 'streaks' and size >= 16 then
        local width = math.max(1.5, size * 0.06)
        for _, share in ipairs({-0.28, -0.08}) do
            local x1, y1 = boxPoint(part, w * share - size * 0.12, h * 0.18, bx, by, cos, sin)
            local x2, y2 = boxPoint(part, w * share + size * 0.12, -h * 0.18, bx, by, cos, sin)
            drawLine(x1, y1, x2, y2, width, material.light, layer)
        end
    elseif detail == 'bevel' and size >= 18 then
        local x, y = boxPoint(part, 0, 0, bx, by, cos, sin)
        local frame = size * 0.2
        shape.borderColor = material.light
        drawShapeAt('rectangle', x, y, w - frame * 2, h - frame * 2, 2, angle, '#00000000', math.max(1.2, size * 0.045), layer)
        shape.borderColor = Look.ink
    end
end

local function drawBox(part, material, look, bx, by, cos, sin, rotation, layer)
    local x, y = point(part, 0, 0, bx, by, cos, sin)
    local angle = rotation + part.angle
    local w, h, size = part.width, part.height, part.size
    local ow = outlineWidth(size)
    local s = math.min(math.max(size * 0.05, 0.6), 3.5)
    local k = ow + s * 1.5 + 0.4 * Look.pixel
    local radius = math.min(look.radius, size * 0.2)
    drawShapeAt('rectangle', x, y, w, h, radius, angle, material.shade, ow, layer)
    if w > k * 2 and h > k * 2 then
        drawShapeAt('rectangle', x - s, y - s, w - k * 2, h - k * 2, math.max(0, radius - k), angle, material.fill, 0, layer)
    end
    boxDetail(part, material, bx, by, cos, sin, angle, ow, layer)
    boxHighlight(x - s, y - s, w, h, angle, k, material.light, layer)
end

-- A round body is lit the same way however it turns, and only its detail, such as the hub of a wheel or the spots of a ball, turns with it.
local function drawCircle(part, material, look, bx, by, cos, sin, rotation, layer)
    local x, y = point(part, 0, 0, bx, by, cos, sin)
    local r = part.radius
    local ow = outlineWidth(part.size)
    local s = math.min(math.max(r * 0.09, 0.6), 4)
    local k = ow + s * 1.5 + 0.4 * Look.pixel
    drawShapeAt('ellipse', x, y, r * 2, r * 2, 0, 0, material.shade, ow, layer)
    order.layer = layer
    if r > k then
        graphics2d.drawCircle(x - s, y - s, r - k, material.fill, order)
    end
    if look.spin and r > 8 then
        for spot = 0, 2 do
            local a = rotation + spot * math.pi * 2 / 3
            drawDot(x + math.cos(a) * r * 0.5, y + math.sin(a) * r * 0.5, r * 0.17, material.light, layer)
        end
    end
    if material.hub then
        local hub = r * 0.48
        drawShapeAt('ellipse', x, y, hub * 2, hub * 2, 0, 0, material.hubShade, math.max(1.2, ow * 0.7), layer)
        drawShapeAt('ellipse', x - hub * 0.12, y - hub * 0.12, hub * 1.5, hub * 1.5, 0, 0, material.hub, 0, layer)
        for bolt = 0, 4 do
            local a = rotation + bolt * math.pi * 2 / 5
            drawDot(x + math.cos(a) * hub * 0.55, y + math.sin(a) * hub * 0.55, math.max(1, hub * 0.13), material.hubShade, layer)
        end
    end
    if r > 6 then
        drawShapeAt('ellipse', x - r * 0.36, y - r * 0.4, r * 0.5, r * 0.26, 0, -0.75, material.light, 0, layer)
    end
end

local function drawCapsule(part, material, look, bx, by, cos, sin, rotation, layer)
    local x, y = point(part, 0, 0, bx, by, cos, sin)
    local angle = rotation + part.angle
    local w, h, size = part.width, part.height, part.size
    local ow = outlineWidth(size)
    local s = math.min(math.max(size * 0.07, 0.6), 3.5)
    local k = ow + s * 1.5 + 0.4 * Look.pixel
    drawShapeAt('rectangle', x, y, w, h, h / 2, angle, material.shade, ow, layer)
    if h > k * 2 then
        drawShapeAt('rectangle', x - s, y - s, w - k * 2, h - k * 2, h / 2 - k, angle, material.fill, 0, layer)
    end
    boxHighlight(x - s, y - s, w, h, angle, k + h * 0.12, material.light, layer)
end

local function transform(source, target, bx, by, cos, sin, dx, dy)
    for index = 1, #source do
        local p, q = source[index], target[index]
        q[1], q[2] = bx + p[1] * cos - p[2] * sin + dx, by + p[1] * sin + p[2] * cos + dy
    end
end

local function drawPolygon(part, material, look, bx, by, cos, sin, layer)
    local ow = outlineWidth(part.size)
    local s = math.min(math.max(part.size * 0.05, 0.6), 3.5)
    local k = ow + s * 1.5 + 0.4 * Look.pixel
    if not part.inset or part.insetWidth ~= k then
        part.inset, part.insetWidth = inset(part.points, k), k
    end
    order.layer = layer
    transform(part.points, part.world, bx, by, cos, sin, 0, 0)
    graphics2d.drawPolygon(part.world, material.shade, order)
    if part.size > k * 2 then
        transform(part.inset, part.lit, bx, by, cos, sin, -s, -s)
        graphics2d.drawPolygon(part.lit, material.fill, order)
    end
    local flat = part.flat
    for index, p in ipairs(part.world) do
        flat[index * 2 - 1], flat[index * 2] = p[1], p[2]
    end
    stroke.width, stroke.color, stroke.closed, stroke.layer = ow, Look.ink, true, layer
    graphics2d.drawStroke(flat, stroke)
end

local function drawLinePart(part, material, look, bx, by, cos, sin, layer)
    local world = part.world
    for index = 1, #part.points, 2 do
        local px, py = part.points[index], part.points[index + 1]
        world[index], world[index + 1] = bx + px * cos - py * sin, by + px * sin + py * cos
    end
    local width = look.thickness
    stroke.closed, stroke.layer = false, layer
    stroke.width, stroke.color = width + outlineWidth(width * 3) * 2, Look.ink
    graphics2d.drawStroke(world, stroke)
    stroke.width, stroke.color = width, material.fill
    graphics2d.drawStroke(world, stroke)
end

local picture = {width = 0, height = 0, rotation = 0, layer = 0}

-- A picture fits a box about as wide as tall and a round body, and stretches over it so its painted outline lies on the edge of the shape.
local function drawPicture(part, material, bx, by, cos, sin, rotation, layer)
    local kind = part.kind
    local width, height = part.width, part.height
    if kind == 'circle' then
        width, height = part.radius * 2, part.radius * 2
    elseif kind ~= 'box' or width > height * 1.45 or height > width * 1.45 then
        return false
    end
    local x, y = point(part, 0, 0, bx, by, cos, sin)
    picture.width, picture.height = width / material.cover[1], height / material.cover[2]
    picture.rotation, picture.layer = rotation + (part.angle or 0), layer
    graphics2d.draw(material.texture, x, y, picture)
    return true
end

local function drawPart(part, material, look, bx, by, cos, sin, rotation, layer)
    if material.texture then
        if drawPicture(part, material, bx, by, cos, sin, rotation, layer) then
            return
        end
        material = material.material
    end
    local kind = part.kind
    if kind == 'box' then
        drawBox(part, material, look, bx, by, cos, sin, rotation, layer)
    elseif kind == 'circle' then
        drawCircle(part, material, look, bx, by, cos, sin, rotation, layer)
    elseif kind == 'capsule' then
        drawCapsule(part, material, look, bx, by, cos, sin, rotation, layer)
    elseif kind == 'polygon' then
        drawPolygon(part, material, look, bx, by, cos, sin, layer)
    else
        drawLinePart(part, material, look, bx, by, cos, sin, layer)
    end
end

-- Draws a dressed body where it is, at the layer of `drawOrder`. Bodies that are gone or were never dressed draw nothing.
function Look.draw(body, drawOrder)
    if not body.valid or type(body.data) ~= 'table' or not body.data.look then
        return
    end
    local look = body.data.look
    local layer = drawOrder and drawOrder.layer or 0
    local bx, by, rotation = body.x, body.y, body.rotation
    local cos, sin = math.cos(rotation), math.sin(rotation)
    for _, part in ipairs(look.parts) do
        drawPart(part, look.material, look, bx, by, cos, sin, rotation, layer)
    end
end

function Look.drawAll(bodies, drawOrder)
    for _, body in ipairs(bodies) do
        Look.draw(body, drawOrder)
    end
end

-- Draws a line with the bold outline of the art, such as a rope, a cable or a rod between two points.
function Look.line(x1, y1, x2, y2, width, color, layer)
    local ow = math.min(Look.outlinePixels * Look.pixel * 0.8, width * 0.5)
    drawLine(x1, y1, x2, y2, width + ow * 2, Look.ink, layer or 0)
    drawLine(x1, y1, x2, y2, width, color, layer or 0)
end

local ropeStroke = {width = 1, texture = nil, textureMode = 'tile', textureLength = 1, join = 'round', cap = 'round', layer = 0}

-- Draws a braided rope through `points`, a list `{x1, y1, x2, y2, ...}`, `width` units thick, with its painted braid repeating along it.
function Look.rope(points, width, layer)
    if not ropeStroke.texture then
        ropeStroke.texture = assets.texture('sprites/images/rope.png', {filter = 'linear', wrap = 'repeat'})
    end
    ropeStroke.width, ropeStroke.textureLength, ropeStroke.layer = width, width * 512 / 44, layer or 0
    graphics2d.drawStroke(points, ropeStroke)
end

local coil = {}
local coilStroke = {width = 1, color = '#FFFFFFFF', join = 'round', cap = 'round', count = 2, layer = 0}

-- Draws a coil spring from one point to another with `coils` turns, which stretches and squeezes with the distance between them.
function Look.spring(x1, y1, x2, y2, width, coils, color, layer)
    local dx, dy = x2 - x1, y2 - y1
    local length = math.max(math.sqrt(dx * dx + dy * dy), 1e-3)
    local ux, uy = dx / length, dy / length
    local nx, ny = -uy, ux
    local lead = length * 0.12
    local steps = coils * 2
    coil[1], coil[2] = x1, y1
    coil[3], coil[4] = x1 + ux * lead, y1 + uy * lead
    for index = 1, steps do
        local along = lead + (length - lead * 2) * (index - 0.5) / steps
        local side = (index % 2 == 1 and 1 or -1) * width / 2
        coil[index * 2 + 3], coil[index * 2 + 4] = x1 + ux * along + nx * side, y1 + uy * along + ny * side
    end
    local count = steps + 4
    coil[count * 2 - 3], coil[count * 2 - 2] = x2 - ux * lead, y2 - uy * lead
    coil[count * 2 - 1], coil[count * 2] = x2, y2
    local thickness = math.max(2 * Look.pixel, width * 0.16)
    coilStroke.count, coilStroke.layer = count, layer or 0
    coilStroke.width, coilStroke.color = thickness + math.min(Look.outlinePixels * Look.pixel * 0.8, thickness) * 2, Look.ink
    graphics2d.drawStroke(coil, coilStroke)
    coilStroke.width, coilStroke.color = thickness, color or '#FFA9BCD3'
    graphics2d.drawStroke(coil, coilStroke)
end

local dash = {0, 0, 0, 0}
local dashStroke = {width = 1, color = '#FFFFFFFF', cap = 'round', dashes = {1, 1}, layer = 0}

-- Draws a dashed guide line, such as the path of a mover or the pull of a joint, whose dashes keep their size on the screen.
function Look.dashed(x1, y1, x2, y2, color, layer)
    dash[1], dash[2], dash[3], dash[4] = x1, y1, x2, y2
    local unit = Look.pixel
    dashStroke.width, dashStroke.color, dashStroke.layer = 3 * unit, color or '#CC2F3352', layer or 0
    dashStroke.dashes[1], dashStroke.dashes[2] = 10 * unit, 8 * unit
    graphics2d.drawStroke(dash, dashStroke)
end

-- Draws a pin, a bolt or a pivot: a small metal disc with the bold outline.
function Look.pin(x, y, radius, layer, color)
    local ow = math.min(Look.outlinePixels * Look.pixel * 0.8, radius * 0.4)
    drawShapeAt('ellipse', x, y, radius * 2, radius * 2, 0, 0, color or '#FFA9BCD3', ow, layer or 0)
    drawDot(x - radius * 0.25, y - radius * 0.25, radius * 0.35, '#FFE9F2FB', layer or 0)
end

local block = {kind = 'box', x = 0, y = 0, angle = 0, width = 0, height = 0, size = 0}
local blockLook = {radius = 3}

-- Draws a rounded block of a material that no body holds, such as a support, a bracket or a frame, with the look of a dressed box.
function Look.block(x, y, width, height, material, layer, rotation)
    block.width, block.height, block.size = width, height, math.min(width, height)
    rotation = rotation or 0
    drawPart(block, Look.material(material), blockLook, x, y, math.cos(rotation), math.sin(rotation), rotation, layer or 0)
end

local disc = {kind = 'circle', x = 0, y = 0, radius = 0, size = 0}
local discLook = {}

-- Draws a disc of a material that no body holds, such as a roller or a hub of a machine, turned by `rotation`.
function Look.disc(x, y, radius, material, layer, rotation)
    disc.radius, disc.size = radius, radius * 2
    rotation = rotation or 0
    drawPart(disc, Look.material(material), discLook, x, y, math.cos(rotation), math.sin(rotation), rotation, layer or 0)
end

-- Returns a polygon of a material that no body holds, in world units, such as a ramp or a rock face, which `Look.drawShape` draws every frame.
function Look.newShape(points, material)
    return {part = polygonPart(points), material = Look.material(material), look = {}}
end

function Look.drawShape(shape, layer)
    drawPart(shape.part, shape.material, shape.look, 0, 0, 1, 0, 0, layer or 0)
end

return Look
