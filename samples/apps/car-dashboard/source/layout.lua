-- Where every part of the dashboard goes, in design units. A landscape screen shows the navigation and the trip on the left, the gauge with the road in the middle and the energy, the tyres and the assistants on the right, like the screen of a car. A portrait screen stacks the gauge, the navigation and the cards, and a wide one, such as a tablet, sets the gauge beside the navigation. The layout changes only when the screen or the bar at the bottom does.
local Layout = {}
Layout.__index = Layout

Layout.margin = 28
Layout.gap = 22
Layout.statusHeight = 84

-- A card wider than this, such as a card of the sides of a landscape screen, shows its readouts beside its drawing, and a narrower one, such as half of a portrait screen, stacks them.
Layout.wideCard = 520

-- In portrait the cards keep the height their content needs, the navigation takes the rest with at least this share of the screen, and the gauge shrinks on a short screen so the map keeps it. A portrait screen at least this wide for its height sets the gauge beside the navigation.
Layout.portraitRows = {energy = 170, cards = 230, stackedCards = 300, trip = 190}
Layout.portraitNavigation = 0.26
Layout.besideShare = 0.7
Layout.stripHeight = 92

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

    self.strip = {x = center.x + center.width * 0.04, y = bottom - Layout.stripHeight, width = center.width * 0.92, height = Layout.stripHeight}
    local radius = math.min(center.width * 0.48, (self.strip.y - top - gap) * 0.5)
    self.gauge = {x = center.x + center.width * 0.5, y = top + radius + 6, radius = radius}
    self.road = {x = center.x, y = self.gauge.y + radius * 0.12, width = center.width, height = self.strip.y - gap - (self.gauge.y + radius * 0.12)}
end

-- A portrait screen gives the gauge the top, then the navigation over the whole width, or beside the gauge on a wide screen, then the cards two by two, the battery beside the energy use and the tyres beside the assistants, and the trip.
function Layout:layOutPortrait(left, top, right, bottom)
    local gap = Layout.gap
    local width, height = right - left, bottom - top
    local half = (width - gap) * 0.5
    local rows = Layout.portraitRows
    local cardHeight = half > Layout.wideCard and rows.cards or rows.stackedCards
    local cards = rows.energy + cardHeight + rows.trip + 4 * gap
    local upper = height - cards

    if width >= height * Layout.besideShare then
        local radius = math.min(half * 0.46, Layout.clusterRadius(upper))
        local clusterTop = top + (upper - Layout.clusterHeight(radius)) * 0.5
        self:layOutCluster(left, clusterTop, half, radius)
        self.navigation = {x = left + half + gap, y = top, width = half, height = upper}
    else
        local radius = math.min(width * 0.4, height * 0.155, Layout.clusterRadius(upper - gap - height * Layout.portraitNavigation))
        local clusterBottom = self:layOutCluster(left, top, width, radius)
        self.navigation = {x = left, y = clusterBottom + gap, width = width, height = top + upper - clusterBottom - gap}
    end

    local y = top + upper + gap
    self.battery = {x = left, y = y, width = half, height = rows.energy}
    self.consumption = {x = left + half + gap, y = y, width = half, height = rows.energy}
    y = y + rows.energy + gap
    self.tyres = {x = left, y = y, width = half, height = cardHeight}
    self.assistants = {x = left + half + gap, y = y, width = half, height = cardHeight}
    y = y + cardHeight + gap
    self.trip = {x = left, y = y, width = width, height = bottom - y}
end

-- The height that the gauge of a radius takes with the road under it and the strip of the assistants below, and the radius that fits a height.
function Layout.clusterHeight(radius)
    return 6 + radius * 2.04 + Layout.stripHeight
end

function Layout.clusterRadius(height)
    return (height - 6 - Layout.stripHeight) / 2.04
end

-- Places the gauge of a radius at the top of an area of a width, the road under it and the strip of the assistants below, and returns the bottom of the strip.
function Layout:layOutCluster(x, y, width, radius)
    local gap = Layout.gap
    self.gauge = {x = x + width * 0.5, y = y + radius + 6, radius = radius}
    self.strip = {x = x + width * 0.04, y = self.gauge.y + radius * 1.04, width = width * 0.92, height = Layout.stripHeight}
    local roadTop = self.gauge.y + radius * 0.12
    self.road = {x = x, y = roadTop, width = width, height = self.strip.y - gap - roadTop}
    return self.strip.y + Layout.stripHeight
end

return Layout
