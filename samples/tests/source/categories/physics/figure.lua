-- The look of the ragdolls of the physics tests: every capsule of a figure dressed in skin, a shirt or trousers, the head with eyes and hair, and the limbs of the far side behind the body in a darker shade, so a figure reads as a person whatever pose it falls into.
local graphics2d = require('haylen.graphics2d')

local Look = require('categories.physics.look')

local Figure = {}

local function cloth(fill, shade, light)
    return Look.tint('wood', fill, shade, light)
end

-- Outfits in the colors of the heroes of the project.
Figure.outfits = {
    {shirt = cloth('#FF2F86F6', '#FF1E5FB8', '#FF96C6FF'), trousers = cloth('#FF3A3F6E', '#FF262A4E', '#FF6A70A0'), hair = '#FF5A3A22'},
    {shirt = cloth('#FFFFC93C', '#FFD99A12', '#FFFFF0A0'), trousers = cloth('#FF2FB3C4', '#FF1E8494', '#FF9BE9F2'), hair = '#FF2A2238'},
    {shirt = cloth('#FF9A6BE0', '#FF6E44B0', '#FFD6BEFF'), trousers = cloth('#FF5A6070', '#FF3E4250', '#FF8A90A0'), hair = '#FFE0A040'},
    {shirt = cloth('#FFF0473E', '#FFB9302A', '#FFFF9A92'), trousers = cloth('#FF6A4A3A', '#FF4A3226', '#FF9A7A66'), hair = '#FF8A3A1E'},
}
for _, outfit in ipairs(Figure.outfits) do
    outfit.shirt.detail, outfit.trousers.detail = nil, nil
    outfit.farShirt = cloth(outfit.shirt.shade, outfit.shirt.shade, outfit.shirt.fill)
    outfit.farTrousers = cloth(outfit.trousers.shade, outfit.trousers.shade, outfit.trousers.fill)
    outfit.farShirt.detail, outfit.farTrousers.detail = nil, nil
end

-- The parts from the back to the front: the far arm and leg, the body and the head, then the near leg and arm.
Figure.order = {'upperArmLeft', 'lowerArmLeft', 'upperLegLeft', 'lowerLegLeft', 'hips', 'chest', 'head', 'upperLegRight', 'lowerLegRight', 'upperArmRight', 'lowerArmRight'}

local function materialOf(part, outfit)
    if part == 'head' then
        return 'skin'
    end
    local far = part:find('Left$') ~= nil
    if part:find('Arm') or part == 'chest' then
        return far and outfit.farShirt or outfit.shirt
    end
    return far and outfit.farTrousers or outfit.trousers
end

-- Dresses every part of a ragdoll in an outfit of `Figure.outfits`, chosen by `index`, and returns the outfit, which `Figure.draw` takes.
function Figure.dress(doll, index)
    local outfit = Figure.outfits[(index - 1) % #Figure.outfits + 1]
    for part, body in pairs(doll:bodies()) do
        Look.dress(body, materialOf(part, outfit))
        body.data.part = part
    end
    return outfit
end

local hair = {kind = 'ellipse', radius = 0, rotation = 0, color = '#FF000000', borderWidth = 0, borderColor = '#FF1E1B33', startAngle = 0, sweep = math.pi, layer = 0}
local area = {0, 0, 0, 0}
local eye = {layer = 0}
local partOrder = {layer = 0}

-- Draws a dressed ragdoll at `layer`, with the face on its head.
function Figure.draw(doll, outfit, layer)
    if not doll.valid then
        return
    end
    local bodies = doll:bodies()
    partOrder.layer = layer
    for _, part in ipairs(Figure.order) do
        Look.draw(bodies[part], partOrder)
    end

    -- The hair covers the back and the top of the head, and the eyes look the way the chest faces.
    local head = bodies.head
    local size = head.data.look.parts[1].size
    local rotation = head.rotation
    local cos, sin = math.cos(rotation), math.sin(rotation)
    local upX, upY = sin, -cos
    local cx, cy = head.x + upX * size * 0.18, head.y + upY * size * 0.18
    area[1], area[2], area[3], area[4] = cx - size * 0.56, cy - size * 0.5, size * 1.12, size * 1.0
    hair.color, hair.layer = outfit.hair, layer
    hair.rotation, hair.startAngle, hair.sweep = rotation, math.pi * 0.95, math.pi * 1.25
    hair.borderWidth = math.min(Look.outlinePixels * Look.pixel, size * 0.12)
    graphics2d.drawShape(area, hair)
    eye.layer = layer
    local radius = math.max(1.2 * Look.pixel, size * 0.08)
    for _, ahead in ipairs({0.26, 0.06}) do
        graphics2d.drawCircle(head.x + cos * size * ahead - upX * size * 0.02, head.y + sin * size * ahead - upY * size * 0.02, radius, Look.ink, eye)
    end
end

return Figure
