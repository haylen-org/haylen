-- The base of the tween tests: the replay action, R or the X button, the icons that the tweens move, and lanes where an icon travels along a rounded rail with the marks of where its motion starts and ends and a trail of where it just was, so a still frame shows what the tween does.
local assets = require('haylen.assets')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')

local Test = require('harness.test')

local TweenTest = haylen.class('TweenTest', Test)

TweenTest.replay = {name = 'replay', type = 'button', bindings = {'key:r', 'button:west'}}
TweenTest.labelWidth = 300
TweenTest.laneHeight = 132
TweenTest.trailLength = 14
TweenTest.groove = '#FF262C3C'
TweenTest.grooveEdge = '#FF353D52'
TweenTest.tick = '#FF4A5370'

-- Mounts the frame of the harness and loads the replay action with the `actions` of the options.
function TweenTest:frame(options)
    TweenTest.super.frame(self, options)
    local actions = {TweenTest.replay}
    for _, action in ipairs(options.actions or {}) do
        actions[#actions + 1] = action
    end
    self:loadActions({actions = actions})
end

-- Returns an icon of the fantasy theme, such as `coin`, `gem`, `star`, `heart`, `potion`, `key` or `shield`, painted in high definition.
function TweenTest.icon(name)
    return assets.texture('interface/themes/fantasy/icons/' .. name .. '.png', {filter = 'linear'})
end

-- Splits the area into `count` lanes no taller than `laneHeight`, centered on its height, and returns their rectangles, top to bottom, with the room of the labels on their left, `labelWidth` wide unless `label` says otherwise.
function TweenTest.lanes(area, count, label)
    label = label or TweenTest.labelWidth
    local lanes = {}
    local height = math.min(TweenTest.laneHeight, area.height / count)
    local top = area.y + (area.height - height * count) / 2
    for index = 1, count do
        lanes[index] = {x = area.x + label, y = top + (index - 1) * height, width = area.width - label - 56, height = height, label = label}
    end
    return lanes
end

-- Keeps the last values of a lane, which `drawLane` draws as a trail behind the icon.
function TweenTest.trail()
    return collections.newRingBuffer(TweenTest.trailLength)
end

-- The x of a value on a lane whose rail spans `min` to `max`, 0 to 1000 by default.
function TweenTest.laneX(lane, value, spec)
    local min, max = spec.min or 0, spec.max or 1000
    return lane.x + lane.width * (value - min) / (max - min)
end

-- Draws a lane from `spec`: the `title` and `caption` on the left, a rounded rail with a tick at every value of `ticks`, a hollow mark at `from` and a filled mark at `to` in `color`, the `marks` of extra values, the `trail` of the last values and the `icon` at `value`, as opaque as `alpha`.
function TweenTest.drawLane(lane, spec)
    local middle = lane.y + lane.height / 2
    local unit = graphics2d.canvasUnitSize()
    local labelX = lane.x - lane.label + 24
    graphics2d.drawText(nil, spec.title, labelX, middle - 4, {size = 26, color = Test.ink, anchor = {0, 1}})
    if spec.caption then
        graphics2d.drawText(nil, spec.caption, labelX, middle + 4, {size = 18, color = Test.muted, anchor = {0, 0}, maxWidth = spec.captionWidth or lane.label - 48})
    end

    local left, right = TweenTest.laneX(lane, spec.min or 0, spec), TweenTest.laneX(lane, spec.max or 1000, spec)
    graphics2d.drawShape({left - 14, middle - 7, right - left + 28, 14}, {radius = 7, color = TweenTest.groove, borderWidth = 2 * unit, borderColor = TweenTest.grooveEdge})
    for _, value in ipairs(spec.ticks or {0, 1000}) do
        local x = TweenTest.laneX(lane, value, spec)
        graphics2d.drawShape({x - 1.5, middle + 14, 3, 10}, {radius = 1.5, color = TweenTest.tick})
        graphics2d.drawText(nil, tostring(value), x, middle + 28, {size = 15, color = Test.muted, anchor = {0.5, 0}})
    end

    local color = m.color(spec.color or Test.accent)
    for _, value in ipairs(spec.marks or {}) do
        local x = TweenTest.laneX(lane, value, spec)
        graphics2d.drawShape({x - 4, middle - 4, 8, 8}, {kind = 'ellipse', color = color:withAlpha(0.55)})
    end
    if spec.from and spec.to then
        local from, to = TweenTest.laneX(lane, spec.from, spec), TweenTest.laneX(lane, spec.to, spec)
        graphics2d.drawStroke({from, middle - 34, to, middle - 34}, {width = 3, color = color:withAlpha(0.6), cap = 'round', dashes = {10, 8}})
        local tip = to > from and -1 or 1
        graphics2d.drawPolygon({{to, middle - 34}, {to + tip * 12, middle - 41}, {to + tip * 12, middle - 27}}, color:withAlpha(0.8))
        graphics2d.drawShape({from - 9, middle - 9, 18, 18}, {kind = 'ellipse', color = '#00000000', borderWidth = 3, borderColor = color})
        graphics2d.drawShape({to - 9, middle - 9, 18, 18}, {kind = 'ellipse', color = color, borderWidth = 3, borderColor = '#FF1E1B33'})
    end

    local size = spec.size or 52
    if spec.trail then
        local count = spec.trail.size
        for index = 1, count - 1 do
            local share = index / count
            graphics2d.draw(spec.icon, TweenTest.laneX(lane, spec.trail:get(index), spec), middle, {width = size * (0.5 + share * 0.4), height = size * (0.5 + share * 0.4), pivotX = 0.5, pivotY = 0.5, color = m.color(1, 1, 1, share * 0.22), layer = 1})
        end
    end
    graphics2d.draw(spec.icon, TweenTest.laneX(lane, spec.value, spec), middle, {width = size, height = size, pivotX = 0.5, pivotY = 0.5, rotation = spec.rotation or 0, color = m.color(1, 1, 1, spec.alpha or 1), layer = 2})
end

-- Samples an easing curve, a function or any curve that `m.ease` takes, and returns its values with the lowest and highest of them, never above 0 and never under 1, so a plot that spans them shows the curve whole with its lines of 0 and 1.
function TweenTest.sample(curve, count)
    local values, low, high = {}, 0, 1
    for step = 0, count do
        local value = type(curve) == 'function' and curve(step / count) or m.ease(curve, step / count)
        values[step + 1] = value
        low, high = math.min(low, value), math.max(high, value)
    end
    return values, low, high
end

-- Plots the `values` that `TweenTest.sample` returns, from `low` to `high`, in the frame `rect`, padded so the curve never touches its edges, with dashed lines at 0 and 1 and a dot at the `progress` and `value` of the options. The list `points` of the options holds the points of the stroke from one frame to the next.
function TweenTest.drawPlot(rect, values, low, high, options)
    local x, y, width, height = rect[1], rect[2], rect[3], rect[4]
    graphics2d.drawShape(rect, {radius = 10, color = options.background or '#FF1C2130', borderWidth = 2, borderColor = options.border or Test.line})
    local pad = 10
    local function toY(value)
        return y + pad + (height - pad * 2) * (high - value) / (high - low)
    end
    for _, level in ipairs({0, 1}) do
        graphics2d.drawStroke({x + 6, toY(level), x + width - 6, toY(level)}, {width = 2, color = TweenTest.tick, dashes = {6, 6}})
    end
    local points = options.points
    local count = #values - 1
    for step = 0, count do
        points[step * 2 + 1] = x + pad + (width - pad * 2) * step / count
        points[step * 2 + 2] = toY(values[step + 1])
    end
    graphics2d.pushClip(rect)
    graphics2d.drawStroke(points, {width = 3, color = options.color, join = 'round', cap = 'round', layer = 1})
    if options.progress then
        graphics2d.drawShape({x + pad + (width - pad * 2) * options.progress - 7, toY(options.value) - 7, 14, 14}, {kind = 'ellipse', color = Test.ink, borderWidth = 3, borderColor = '#FF1E1B33', layer = 2})
    end
    graphics2d.popClip()
end

return TweenTest
