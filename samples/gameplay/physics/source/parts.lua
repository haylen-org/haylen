-- Adds shapes to bodies and keeps their outlines in body coordinates, because shapes do not report their geometry, so the tests can draw every body filled.
local graphics2d = require('haylen.graphics2d')

local parts = {}

local kPalette = {'#FFE57373', '#FF64B5F6', '#FF81C784', '#FFFFD54F', '#FFBA68C8', '#FF4DD0E1', '#FFFF8A65', '#FFAED581'}
local kStaticColor = '#FF4F5B6E'
local kOutline = '#66000000'
local kRoundSegments = 12

local painted = 0

local function data(body)
    if body.data == nil then
        painted = painted + 1
        body.data = {parts = {}, color = body.type == 'static' and kStaticColor or kPalette[painted % #kPalette + 1]}
    end
    return body.data
end

local function record(body, part)
    local list = data(body).parts
    list[#list + 1] = part
end

-- Places a list of points like a shape option table does: rotated around the body origin, then offset.
local function place(points, options)
    options = options or {}
    local rotation = options.rotation or 0
    local cos, sin = math.cos(rotation), math.sin(rotation)
    local dx, dy = options.offsetX or 0, options.offsetY or 0
    local flat = {}
    for _, point in ipairs(points) do
        local x, y = point[1] or point.x, point[2] or point.y
        flat[#flat + 1] = x * cos - y * sin + dx
        flat[#flat + 1] = x * sin + y * cos + dy
    end
    return flat
end

local function capsulePoints(x1, y1, x2, y2, radius)
    local angle = math.atan(y2 - y1, x2 - x1)
    local points = {}
    for index = 0, kRoundSegments do
        local a = angle - math.pi / 2 + math.pi * index / kRoundSegments
        points[#points + 1] = {x2 + math.cos(a) * radius, y2 + math.sin(a) * radius}
    end
    for index = 0, kRoundSegments do
        local a = angle + math.pi / 2 + math.pi * index / kRoundSegments
        points[#points + 1] = {x1 + math.cos(a) * radius, y1 + math.sin(a) * radius}
    end
    return points
end

function parts.box(body, width, height, options)
    local w, h = width / 2, height / 2
    record(body, {kind = 'fill', points = place({{-w, -h}, {w, -h}, {w, h}, {-w, h}}, options)})
    return body:addBox(width, height, options)
end

function parts.circle(body, radius, options)
    record(body, {kind = 'circle', x = options and options.offsetX or 0, y = options and options.offsetY or 0, radius = radius})
    return body:addCircle(radius, options)
end

function parts.capsule(body, x1, y1, x2, y2, radius, options)
    record(body, {kind = 'fill', points = place(capsulePoints(x1, y1, x2, y2, radius), options)})
    return body:addCapsule(x1, y1, x2, y2, radius, options)
end

function parts.polygon(body, points, options)
    record(body, {kind = 'fill', points = place(points, options)})
    return body:addPolygon(points, options)
end

function parts.segment(body, x1, y1, x2, y2, options)
    record(body, {kind = 'line', points = place({{x1, y1}, {x2, y2}}, options)})
    return body:addSegment(x1, y1, x2, y2, options)
end

function parts.chain(body, points, loop, options)
    record(body, {kind = 'line', points = place(points, options), closed = loop})
    return body:addChain(points, loop, options)
end

-- Records a filled outline without adding a shape, for bodies that helpers built or for the solid side of a chain.
function parts.outline(body, points)
    record(body, {kind = 'fill', points = place(points)})
end

-- Records a circle without adding a shape.
function parts.round(body, radius, x, y)
    record(body, {kind = 'circle', x = x or 0, y = y or 0, radius = radius})
end

function parts.paint(body, color)
    data(body).color = color
end

function parts.draw(body, order)
    if not body.valid or body.data == nil then
        return
    end
    local color = body.data.color
    local x, y, rotation = body.x, body.y, body.rotation
    local cos, sin = math.cos(rotation), math.sin(rotation)
    for _, part in ipairs(body.data.parts) do
        if part.kind == 'circle' then
            local cx, cy = x + part.x * cos - part.y * sin, y + part.x * sin + part.y * cos
            graphics2d.drawCircle(cx, cy, part.radius, color, order)
            graphics2d.drawRing(cx, cy, part.radius - 1.5, 3, kOutline, order)
            graphics2d.drawLine(cx, cy, cx + cos * part.radius, cy + sin * part.radius, 3, kOutline, order)
        else
            local points, flat = {}, part.points
            for index = 1, #flat, 2 do
                points[#points + 1] = {x + flat[index] * cos - flat[index + 1] * sin, y + flat[index] * sin + flat[index + 1] * cos}
            end
            if part.kind == 'fill' then
                graphics2d.drawPolygon(points, color, order)
                graphics2d.drawPolyline(points, 3, kOutline, true, order)
            else
                graphics2d.drawPolyline(points, 6, color, part.closed, order)
            end
        end
    end
end

function parts.drawAll(bodies, order)
    for _, body in ipairs(bodies) do
        parts.draw(body, order)
    end
end

return parts
