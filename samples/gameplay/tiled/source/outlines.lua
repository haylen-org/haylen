-- The world outlines of Tiled objects, which maps leave to the app to draw, turned by the rotation of each object around its origin.
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')

local outlines = {}

local kRoundSegments = 24

-- The point of a tile object that sits at its position, as fractions of its size, by the object alignment of its tileset.
local kAnchors = {
    unspecified = {0, 1}, bottomleft = {0, 1}, bottom = {0.5, 1}, bottomright = {1, 1},
    left = {0, 0.5}, center = {0.5, 0.5}, right = {1, 0.5},
    topleft = {0, 0}, top = {0.5, 0}, topright = {1, 0},
}

local function ellipse(width, height)
    local points = {}
    for index = 0, kRoundSegments - 1 do
        local angle = index / kRoundSegments * math.pi * 2
        points[#points + 1] = {width / 2 + math.cos(angle) * width / 2, height / 2 + math.sin(angle) * height / 2}
    end
    return points
end

-- A capsule rounds the short sides of its rectangle into half circles.
local function capsule(width, height)
    local points = {}
    local radius = math.min(width, height) / 2
    local horizontal = width >= height
    local half = kRoundSegments // 2
    for side = 0, 1 do
        for index = 0, half do
            local angle = (horizontal and -math.pi / 2 or 0) + side * math.pi + index / half * math.pi
            local cx = horizontal and (side == 0 and width - radius or radius) or width / 2
            local cy = horizontal and height / 2 or (side == 0 and height - radius or radius)
            points[#points + 1] = {cx + math.cos(angle) * radius, cy + math.sin(angle) * radius}
        end
    end
    return points
end

local function tileAnchor(map, object)
    local name = map:tileInfo(object.gid).tileset
    for _, tileset in ipairs(map:tilesets()) do
        if tileset.name == name then
            local alignment = tileset.objectAlignment
            if alignment == 'unspecified' and map.orientation == 'isometric' then
                alignment = 'bottom'
            end
            return kAnchors[alignment]
        end
    end
end

-- Returns the local points of an object and whether they close, or `nil` for points.
local function localPoints(map, object)
    local shape, width, height = object.shape, object.width, object.height
    if shape == 'rectangle' or shape == 'text' then
        return {{0, 0}, {width, 0}, {width, height}, {0, height}}, true
    elseif shape == 'tile' then
        local anchor = tileAnchor(map, object)
        local left, top = -anchor[1] * width, -anchor[2] * height
        return {{left, top}, {left + width, top}, {left + width, top + height}, {left, top + height}}, true
    elseif shape == 'ellipse' then
        return ellipse(width, height), true
    elseif shape == 'capsule' then
        return capsule(width, height), true
    elseif shape == 'polygon' or shape == 'polyline' then
        local points = {}
        for index, point in ipairs(object.points) do
            points[index] = {point.x, point.y}
        end
        return points, shape == 'polygon'
    end
end

-- Returns the world points of the object, whether they close, and nothing for points. The arguments `dx` and `dy` add the offset of its layers.
function outlines.of(map, object, dx, dy)
    local points, closed = localPoints(map, object)
    if points == nil then
        return nil
    end
    local cos, sin = math.cos(object.rotation), math.sin(object.rotation)
    local world = {}
    for index, point in ipairs(points) do
        local x, y = map:objectToWorld(object.x + point[1] * cos - point[2] * sin, object.y + point[1] * sin + point[2] * cos)
        world[index] = {x + (dx or 0), y + (dy or 0)}
    end
    return world, closed
end

function outlines.draw(map, object, color, thickness, order, dx, dy)
    local points, closed = outlines.of(map, object, dx, dy)
    if points then
        graphics2d.drawPolyline(points, thickness, color, closed, order)
        return
    end
    local x, y = map:objectToWorld(object.x, object.y)
    graphics2d.drawRing(x + (dx or 0), y + (dy or 0), 8, thickness, color, order)
end

-- Tells whether a world point lies on the object: inside closed shapes and near lines and points.
function outlines.contains(map, object, x, y, dx, dy)
    local points, closed = outlines.of(map, object, dx, dy)
    if points == nil then
        local ox, oy = map:objectToWorld(object.x, object.y)
        return math.abs(ox + (dx or 0) - x) < 12 and math.abs(oy + (dy or 0) - y) < 12
    end
    if closed then
        return m.polygonContains(points, {x, y})
    end
    for index = 2, #points do
        if m.distanceToSegment({points[index - 1], points[index]}, {x, y}) < 8 then
            return true
        end
    end
    return false
end

return outlines
