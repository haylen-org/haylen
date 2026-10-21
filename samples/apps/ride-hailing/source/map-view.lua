-- The map behind the ride screens: a camera that fits a part of the city into the room the GUI leaves, follows drags, the wheel and pinches, and draws the day map of the city, the traffic, routes, the pickup and the destination, cars, the pulse of the rider and the radar of the search.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local math2d = require('haylen.math')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local City = require('city')
local Rides = require('rides')
local Style = require('style')

local MapView = haylen.class('MapView')

-- Room in UI units that the fitted area keeps around it.
MapView.margin = 96
MapView.minZoom = 0.2
-- The most points of the screen a block of the city spans, so a short route never fills a large window with a few blocks.
MapView.maxBlockPoints = 110

function MapView:init()
    self.camera = graphics2d.newCamera()
    self.camera.zoom = {0.8, 0.8}
    self.zoomFactor = 1
    self.pan = {0, 0}
end

function MapView:reset()
    self.zoomFactor = 1
    self.pan = {0, 0}
end

-- Moves the camera so the rectangle `bounds` of the city shows in `area`, a rectangle of the screen, plus the drags and zooms of the person.
function MapView:fit(bounds, area, dt, instant)
    local visible = viewport.visibleRect()
    self:readPointer(area)
    local _, scale, points = ui.scale()
    local margin = MapView.margin * scale
    local maxZoom = MapView.maxBlockPoints * scale / points / City.blockSize
    local fit = math.min(maxZoom, (area.width - margin * 2) / math.max(bounds.width, 1), (area.height - margin * 2) / math.max(bounds.height, 1))
    local zoom = math.max(MapView.minZoom, math.min(maxZoom * 2, fit * self.zoomFactor))
    self.zoomFactor = zoom / math.max(fit, 0.0001)
    local centerX = bounds.x + bounds.width / 2 + self.pan[1] - (area.x + area.width / 2 - (visible.x + visible.width / 2)) / zoom
    local centerY = bounds.y + bounds.height / 2 + self.pan[2] - (area.y + area.height / 2 - (visible.y + visible.height / 2)) / zoom
    local camera = self.camera
    local blend = instant and 1 or 1 - math.exp(-dt * 8)
    local current = camera.zoom.x + (zoom - camera.zoom.x) * blend
    camera.zoom = {current, current}
    camera.position = {camera.x + (centerX - camera.x) * blend, camera.y + (centerY - camera.y) * blend}
end

-- The bounds of a list of points, with a least size so a single point shows a few blocks around it.
function MapView.bounds(points, least)
    local left, top, right, bottom = math.huge, math.huge, -math.huge, -math.huge
    for index = 1, #points, 2 do
        left, right = math.min(left, points[index]), math.max(right, points[index])
        top, bottom = math.min(top, points[index + 1]), math.max(bottom, points[index + 1])
    end
    least = least or 0
    local width, height = math.max(right - left, least), math.max(bottom - top, least)
    return {x = (left + right - width) / 2, y = (top + bottom - height) / 2, width = width, height = height}
end

-- A drag that starts on the map pans it, a pinch or the wheel zooms it, and presses that start on the interface stay with it.
function MapView:readPointer(area)
    local zoom = self.camera.zoom.x
    local touches = input.touches()
    if #touches == 1 then
        local touch = touches[1]
        if touch.phase == 'began' then
            self.dragging = not ui.usingPointer()
        elseif self.dragging then
            self.pan[1], self.pan[2] = self.pan[1] - touch.dx / zoom, self.pan[2] - touch.dy / zoom
        end
    elseif #touches == 0 then
        if input.mousePressed('left') then
            self.dragging = not ui.usingPointer()
        elseif input.mouseDown('left') and self.dragging then
            local dx, dy = input.mouseDelta()
            self.pan[1], self.pan[2] = self.pan[1] - dx / zoom, self.pan[2] - dy / zoom
        end
    end
    local _, wheel = input.mouseScroll()
    if wheel ~= 0 and not ui.usingPointer() then
        self.zoomFactor = self.zoomFactor * (wheel > 0 and 1.1 or 1 / 1.1)
    end
    for _, gesture in ipairs(input.gestures()) do
        if gesture.type == 'pinch' then
            self.pinchBase = self.pinchBase or self.zoomFactor
            self.zoomFactor = self.pinchBase * gesture.scale
        end
    end
    if #touches < 2 then
        self.pinchBase = nil
    end
end

-- Begins the world canvas of the map. The marks keep their size in UI units, so they stay as large on the screen as the interface around them at every zoom and density.
function MapView:begin()
    graphics2d.beginWorld(self.camera)
    local _, scale = ui.scale()
    self.unit = scale / self.camera.zoom.x
    Rides.city:draw(Style.map, self.camera:visibleBounds())
end

-- The cars of the city seen from above, white with their windows, the same size on the screen at every zoom.
function MapView:drawTraffic()
    for _, car in ipairs(Rides.traffic.cars) do
        if car.x then
            self:drawCar(car.x, car.y, car.dx, car.dy, car.color, 1)
        end
    end
end

-- A car seen from above, turned along its direction: the body with an outline, the windshield and the rear window, and a soft shadow under it.
function MapView:drawCar(x, y, dx, dy, color, scale)
    local unit = self.unit * (scale or 1)
    local angle = (dx ~= 0 or dy ~= 0) and math.atan(dy, dx) or 0
    local length, width = 34 * unit, 18 * unit
    graphics2d.drawShape({x - length / 2 + 2 * unit, y - width / 2 + 3 * unit, length, width}, {radius = 7 * unit, rotation = angle, color = '#3315161A'})
    graphics2d.drawShape({x - length / 2, y - width / 2, length, width}, {radius = 7 * unit, rotation = angle, color = color, borderWidth = 2.5 * unit, borderColor = Style.color.ink})
    local cos, sin = math.cos(angle), math.sin(angle)
    local frontX, frontY = x + cos * 6 * unit, y + sin * 6 * unit
    graphics2d.drawShape({frontX - 3 * unit, frontY - 6.5 * unit, 6 * unit, 13 * unit}, {radius = 2 * unit, rotation = angle, color = '#FF2B3446'})
    local backX, backY = x - cos * 10 * unit, y - sin * 10 * unit
    graphics2d.drawShape({backX - 2 * unit, backY - 5.5 * unit, 4 * unit, 11 * unit}, {radius = 1.5 * unit, rotation = angle, color = '#FF2B3446'})
end

-- The route: a white casing under a dark line, and its part already ridden in gray.
function MapView:drawRoute(points, walked)
    local unit = self.unit
    graphics2d.drawStroke(points, {width = 16 * unit, color = '#FFFFFFFF', join = 'round', cap = 'round'})
    graphics2d.drawStroke(points, {width = 9 * unit, color = walked and '#FFB4B1A8' or Style.color.ink, join = 'round', cap = 'round'})
    if walked then
        local rest = City.routeFrom(points, walked)
        if #rest >= 4 then
            graphics2d.drawStroke(rest, {width = 9 * unit, color = Style.color.ink, join = 'round', cap = 'round'})
        end
    end
end

-- The pickup: a round dark pin with a white heart, and its label in a dark bubble beside it.
function MapView:drawPickup(x, y, label, font)
    local unit = self.unit
    graphics2d.drawCircle(x, y, 15 * unit, '#FFFFFFFF')
    graphics2d.drawCircle(x, y, 11 * unit, Style.color.ink)
    graphics2d.drawCircle(x, y, 4.5 * unit, '#FFFFFFFF')
    self:drawLabel(x, y, label, font)
end

-- The destination: a square dark pin with a white center.
function MapView:drawDestination(x, y, label, font)
    local unit = self.unit
    graphics2d.drawShape({x - 15 * unit, y - 15 * unit, 30 * unit, 30 * unit}, {radius = 5 * unit, color = '#FFFFFFFF'})
    graphics2d.drawShape({x - 11 * unit, y - 11 * unit, 22 * unit, 22 * unit}, {radius = 3 * unit, color = Style.color.ink})
    graphics2d.drawShape({x - 4.5 * unit, y - 4.5 * unit, 9 * unit, 9 * unit}, {radius = 1.5 * unit, color = '#FFFFFFFF'})
    self:drawLabel(x, y, label, font)
end

-- A label in a dark bubble above a point of the map.
function MapView:drawLabel(x, y, label, font)
    if not label then
        return
    end
    local unit = self.unit
    local size = 22 * unit
    local width = graphics2d.measureText(font, label, {size = size}) + 28 * unit
    local left, top = x - width / 2, y - 64 * unit
    graphics2d.drawShape({left, top, width, 40 * unit}, {radius = 12 * unit, color = Style.color.ink})
    graphics2d.drawPolygon({{x - 8 * unit, top + 39 * unit}, {x + 8 * unit, top + 39 * unit}, {x, top + 48 * unit}}, Style.color.ink)
    graphics2d.drawText(font, label, x, top + 20 * unit, {size = size, color = '#FFFFFFFF', anchor = {0.5, 0.5}})
end

-- The rider: a dot with rings that grow and fade.
function MapView:drawPulse(x, y)
    local unit = self.unit
    local time = haylen.elapsed()
    for ring = 0, 1 do
        local phase = (time * 0.8 + ring * 0.5) % 1
        graphics2d.drawCircle(x, y, (14 + phase * 46) * unit, math2d.color(0.18, 0.42, 1, 0.3 * (1 - phase)))
    end
    graphics2d.drawCircle(x, y, 14 * unit, '#FFFFFFFF')
    graphics2d.drawCircle(x, y, 10 * unit, '#FF2F6BFF')
end

-- The radar of the search for a driver: a sweep in the yellow of the app that turns around the pickup.
function MapView:drawRadar(x, y)
    local unit = self.unit
    local angle = haylen.elapsed() * 3
    local radius = 160 * unit
    local yellow = math2d.color(Style.color.yellow)
    graphics2d.drawCircle(x, y, radius, math2d.color(yellow.r, yellow.g, yellow.b, 0.12))
    graphics2d.drawRing(x, y, radius, 2.5 * unit, math2d.color(yellow.r, yellow.g, yellow.b, 0.9))
    graphics2d.drawRing(x, y, radius * 0.55, 2 * unit, math2d.color(yellow.r, yellow.g, yellow.b, 0.6))
    graphics2d.drawShape({x - radius, y - radius, radius * 2, radius * 2}, {radius = radius, startAngle = angle, sweep = 0.9, color = math2d.color(yellow.r, yellow.g, yellow.b, 0.35)})
end

return MapView
