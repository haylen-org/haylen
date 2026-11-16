-- Where every part of the dashboard goes, in design units. A landscape screen shows the navigation and the trip on the left, the gauge with the road in the middle and the energy, the tyres and the assistants on the right, like the screen of a car. A portrait screen stacks the gauge, the navigation and the cards. The layout changes only when the screen or the bar at the bottom does.
local Layout = {}
Layout.__index = Layout

Layout.margin = 28
Layout.gap = 22
Layout.statusHeight = 84

function Layout.new()
    return setmetatable({}, Layout)
end

-- Lays the parts out again when the safe area or the top of the bar of the controls changed, and returns whether it did.
function Layout:update(safe, barTop)
    if safe.x == self.safeX and safe.y == self.safeY and safe.width == self.safeWidth and safe.height == self.safeHeight and barTop == self.barTop then
        return false
    end
    self.safeX, self.safeY, self.safeWidth, self.safeHeight, self.barTop = safe.x, safe.y, safe.width, safe.height, barTop
    self.landscape = safe.width >= safe.height * 1.2
    local margin, gap = Layout.margin, Layout.gap
    local left, right = safe.x + margin, safe.x + safe.width - margin
    self.status = {x = left, y = safe.y, width = right - left, height = Layout.statusHeight}
    local top = safe.y + Layout.statusHeight + gap * 0.5
    local bottom = barTop - gap
    if self.landscape then
        self:layOutLandscape(left, top, right, bottom)
    else
        self:layOutPortrait(left, top, right, bottom)
    end
    return true
end

function Layout:layOutLandscape(left, top, right, bottom)
    local gap = Layout.gap
    local width, height = right - left, bottom - top
    local side = math.floor(width * 0.29)
    local center = {x = left + side + gap, width = width - 2 * (side + gap)}

    local navigation = math.floor(height * 0.64)
    self.navigation = {x = left, y = top, width = side, height = navigation}
    self.trip = {x = left, y = top + navigation + gap, width = side, height = height - navigation - gap}

    local x = right - side
    local heights = {0.2, 0.24, 0.3}
    local y = top
    local cards = {}
    for index, share in ipairs(heights) do
        cards[index] = {x = x, y = y, width = side, height = math.floor(height * share) - gap}
        y = y + cards[index].height + gap
    end
    self.battery, self.consumption, self.tyres = cards[1], cards[2], cards[3]
    self.assistants = {x = x, y = y, width = side, height = bottom - y}

    self.strip = {x = center.x + center.width * 0.04, y = bottom - 92, width = center.width * 0.92, height = 92}
    local radius = math.min(center.width * 0.48, (self.strip.y - top - gap) * 0.5)
    self.gauge = {x = center.x + center.width * 0.5, y = top + radius + 6, radius = radius}
    self.road = {x = center.x, y = self.gauge.y + radius * 0.12, width = center.width, height = self.strip.y - gap - (self.gauge.y + radius * 0.12)}
end

-- A portrait screen gives the gauge the top, then the navigation over the whole width, then the cards two by two, the battery beside the energy use and the tyres beside the assistants, and the trip.
function Layout:layOutPortrait(left, top, right, bottom)
    local gap = Layout.gap
    local width, height = right - left, bottom - top
    local half = (width - gap) * 0.5
    local radius = math.min(width * 0.4, height * 0.155)
    self.gauge = {x = left + width * 0.5, y = top + radius + 6, radius = radius}
    self.strip = {x = left + width * 0.08, y = self.gauge.y + radius * 1.04, width = width * 0.84, height = 92}
    self.road = {x = left, y = self.gauge.y + radius * 0.12, width = width, height = self.strip.y - gap - (self.gauge.y + radius * 0.12)}

    local y = self.strip.y + self.strip.height + gap
    local rest = bottom - y - 3 * gap
    local navigation = math.floor(rest * 0.4)
    self.navigation = {x = left, y = y, width = width, height = navigation}
    y = y + navigation + gap
    local energy = math.floor(rest * 0.17)
    self.battery = {x = left, y = y, width = half, height = energy}
    self.consumption = {x = left + half + gap, y = y, width = half, height = energy}
    y = y + energy + gap
    local cards = math.floor(rest * 0.26)
    self.tyres = {x = left, y = y, width = half, height = cards}
    self.assistants = {x = left + half + gap, y = y, width = half, height = cards}
    y = y + cards + gap
    self.trip = {x = left, y = y, width = width, height = bottom - y}
end

return Layout
