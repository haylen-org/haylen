-- The map behind the ride screens: a camera that fits a part of the city into the room the GUI leaves, follows drags, the wheel and pinches, and draws the city, the traffic, routes, pins, cars, the pulse of the rider and the radar of the search.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local math2d = require('haylen.math')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local City = require('city')
local Rides = require('rides')

local MapView = haylen.class('MapView')

-- Room in design units that the fitted area keeps around it.
MapView.margin = 110
MapView.minZoom = 0.2
MapView.maxZoom = 3

function MapView:init()
    self.camera = graphics2d.newCamera()
    self.camera.zoom = {0.8, 0.8}
    self.zoomFactor = 1
    self.pan = {0, 0}
    self.icons = {
        pin = assets.vectorImage('icons/pin.svg'),
        flag = assets.vectorImage('icons/flag.svg'),
    }
end

function MapView:reset()
    self.zoomFactor = 1
    self.pan = {0, 0}
end

-- Moves the camera so the rectangle `bounds` of the city shows in `area`, a rectangle of the screen, plus the drags and zooms of the person.
function MapView:fit(bounds, area, dt, instant)
    local visible = viewport.visibleRect()
    self:readPointer(area)
    local fit = math.min((area.width - MapView.margin * 2) / math.max(bounds.width, 1), (area.height - MapView.margin * 2) / math.max(bounds.height, 1))
    local zoom = math.max(MapView.minZoom, math.min(MapView.maxZoom, fit * self.zoomFactor))
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

function MapView:begin()
    graphics2d.beginWorld(self.camera)
    self.unit = 1 / self.camera.zoom.x
    local palette = ui.theme() == 'rideDay' and City.light or City.dark
    Rides.city:draw(palette, self.camera:visibleBounds())
end

-- The cars of the city seen from above, with their headlights.
function MapView:drawTraffic()
    for _, car in ipairs(Rides.traffic.cars) do
        if car.x then
            self:drawCar(car.x, car.y, car.dx, car.dy, car.color, 1)
        end
    end
end

-- A car seen from above, turned along its direction, the same size on the screen at every zoom.
function MapView:drawCar(x, y, dx, dy, color, scale)
    local unit = self.unit * (scale or 1)
    local angle = (dx ~= 0 or dy ~= 0) and math.atan(dy, dx) or 0
    local length, width = 30 * unit, 16 * unit
    graphics2d.drawShape({x - length / 2, y - width / 2, length, width}, {radius = 6 * unit, rotation = angle, color = color, borderWidth = 2.5 * unit, borderColor = '#FF1E1B33'})
    local cos, sin = math.cos(angle), math.sin(angle)
    local frontX, frontY = x + cos * 13 * unit, y + sin * 13 * unit
    graphics2d.drawCircle(frontX - sin * 5 * unit, frontY + cos * 5 * unit, 2.5 * unit, '#FFFFF3B0')
    graphics2d.drawCircle(frontX + sin * 5 * unit, frontY - cos * 5 * unit, 2.5 * unit, '#FFFFF3B0')
end

-- The route with an outline, and its part from `walked` on in the color of the accent.
function MapView:drawRoute(points, walked, color)
    local unit = self.unit
    graphics2d.drawStroke(points, {width = 15 * unit, color = '#FF1E1B33', join = 'round', cap = 'round'})
    graphics2d.drawStroke(points, {width = 9 * unit, color = walked and '#FF5C6A90' or color, join = 'round', cap = 'round'})
    if walked then
        local rest = City.routeFrom(points, walked)
        if #rest >= 4 then
            graphics2d.drawStroke(rest, {width = 9 * unit, color = color, join = 'round', cap = 'round'})
        end
    end
end

-- A pin with an icon, standing on its point, and an optional label in a bubble beside it.
function MapView:drawPin(x, y, icon, color, label, font)
    local unit = self.unit
    graphics2d.drawCircle(x, y, 7 * unit, '#FF1E1B33')
    graphics2d.drawCircle(x, y, 4 * unit, color)
    graphics2d.drawLine(x, y, x, y - 26 * unit, 4 * unit, '#FF1E1B33')
    graphics2d.drawShape({x - 20 * unit, y - 66 * unit, 40 * unit, 40 * unit}, {radius = 20 * unit, color = color, borderWidth = 3.5 * unit, borderColor = '#FF1E1B33'})
    graphics2d.drawVector(icon, x, y - 46 * unit, {width = 22 * unit, height = 22 * unit, color = '#FF1E1B33'})
    if label then
        local size = 19 * unit
        local width = graphics2d.measureText(font, label, {size = size}) + 24 * unit
        graphics2d.drawShape({x + 26 * unit, y - 64 * unit, width, 34 * unit}, {radius = 10 * unit, color = '#FF1E1B33'})
        graphics2d.drawText(font, label, x + 38 * unit, y - 47 * unit, {size = size, color = '#FFFFFFFF', anchor = {0, 0.5}})
    end
end

-- The rider: a dot with rings that grow and fade.
function MapView:drawPulse(x, y, color)
    local unit = self.unit
    local time = haylen.elapsed()
    for ring = 0, 1 do
        local phase = (time * 0.8 + ring * 0.5) % 1
        graphics2d.drawCircle(x, y, (14 + phase * 46) * unit, math2d.color(color.r, color.g, color.b, 0.35 * (1 - phase)))
    end
    graphics2d.drawCircle(x, y, 13 * unit, '#FFFFFFFF')
    graphics2d.drawCircle(x, y, 9 * unit, color)
end

-- The radar of the search for a driver: a sweep that turns around the pickup.
function MapView:drawRadar(x, y, color)
    local unit = self.unit
    local angle = haylen.elapsed() * 3
    local radius = 150 * unit
    graphics2d.drawCircle(x, y, radius, math2d.color(color.r, color.g, color.b, 0.08))
    graphics2d.drawRing(x, y, radius, 2 * unit, math2d.color(color.r, color.g, color.b, 0.5))
    graphics2d.drawRing(x, y, radius * 0.55, 2 * unit, math2d.color(color.r, color.g, color.b, 0.3))
    graphics2d.drawShape({x - radius, y - radius, radius * 2, radius * 2}, {radius = radius, startAngle = angle, sweep = 0.9, color = math2d.color(color.r, color.g, color.b, 0.28)})
end

return MapView
