-- The cards around the gauge: the status bar, the navigation with the next turn and the moving map, the trip computer, the battery, the chart of the energy use, the tyres and the tiles of the driver assistants.
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')

local Drive = require('drive')
local Style = require('style')
local Text = require('text')

local Panels = {}
Panels.__index = Panels

-- The meters of the city that the map shows across its width, and the streets of the route it draws.
Panels.mapSpan = 1500

-- A card wider than this, such as a card of the sides of a landscape screen, shows its readouts beside its drawing, and a narrower one, such as half of a portrait screen, stacks them.
Panels.wideCard = 520
Panels.routeSegments = 4
Panels.padding = 28

function Panels.new(images, icons)
    local self = setmetatable({images = images, icons = icons}, Panels)
    self.route = {}
    for index = 1, (Panels.routeSegments + 1) * 2 do
        self.route[index] = 0
    end
    self.chartLine = {}
    self.chartArea = {}
    self.chartIndices = {}
    for index = 1, Drive.historySize do
        self.chartLine[index] = {0, 0}
        self.chartArea[index * 2 - 1] = {x = 0, y = 0, color = '#6622C55E'}
        self.chartArea[index * 2] = {x = 0, y = 0, color = '#0022C55E'}
    end
    for index = 1, Drive.historySize - 1 do
        local top, bottom = index * 2 - 1, index * 2
        local base = #self.chartIndices
        self.chartIndices[base + 1], self.chartIndices[base + 2], self.chartIndices[base + 3] = top, top + 2, bottom
        self.chartIndices[base + 4], self.chartIndices[base + 5], self.chartIndices[base + 6] = top + 2, bottom + 2, bottom
    end
    self.arrow = graphics2d.newPath()
    self.arrowStroke = {width = 9, color = Style.color.text, cap = 'round', join = 'round'}
    self.markerOptions = {kind = 'ellipse', borderWidth = 5, borderColor = '#FFFFFFFF'}
    self.routeOptions = {width = 14, color = Style.color.cyan, cap = 'round', join = 'round'}
    self.topOptions = {pivotX = 0.5, pivotY = 0.5}
    self.mapClip = {0, 0, 0, 0}
    self.drawMapShape = function()
        local clip = self.mapClip
        Style.drawShape(clip[1], clip[2], clip[3], clip[4], 22, '#FFFFFFFF')
    end
    return self
end

-- The status bar: the weather and the cabin on the left, the drive mode in the middle, and the connections and the clock on the right.
function Panels:drawStatus(status, drive, climate)
    local icons = self.icons
    local middle = status.y + status.height * 0.5
    local x = status.x + 8
    Style.drawIcon(icons.weather, x + 26, middle, 52, Style.color.text)
    Style.drawText('24°C', x + 64, middle - 14, Style.type.label, Style.color.text, 0, 0.5, true)
    Style.drawText('Outside', x + 64, middle + 18, Style.type.small * 0.8, Style.color.muted, 0, 0.5)
    x = x + 210
    Style.drawIcon(icons.thermometer, x + 22, middle, 46, Style.color.text)
    Style.drawText(Text.format('%.1f°C', climate:cabin(), 0.1), x + 56, middle - 14, Style.type.label, Style.color.text, 0, 0.5, true)
    Style.drawText('Cabin', x + 56, middle + 18, Style.type.small * 0.8, Style.color.muted, 0, 0.5)

    local center = status.x + status.width * 0.5
    Style.drawShape(center - 120, middle - 26, 240, 52, 26, '#FF111B33', Style.cardEdge)
    Style.drawText('Comfort', center, middle, Style.type.small, Style.color.cyan, 0.5, 0.5, true)

    local right = status.x + status.width - 8
    Style.drawText(Text.clock(drive.clock), right, middle, Style.type.value, Style.color.text, 1, 0.5, true)
    local clockWidth = Style.measureText(Text.clock(drive.clock), Style.type.value, true)
    x = right - clockWidth - 40
    for bar = 4, 1, -1 do
        local height = 10 + bar * 7
        Style.drawShape(x - 8, middle + 18 - height, 8, height, 3, Style.color.text)
        x = x - 13
    end
    Style.drawText('5G', x - 4, middle + 4, Style.type.small, Style.color.text, 1, 0.5, true)
    Style.drawIcon(icons.wifi, x - 80, middle, 42, Style.color.text)
    Style.drawIcon(icons.bluetooth, x - 136, middle, 40, Style.color.text)
end

-- Builds the arrow of the next turn once its side changes: up the street and around the corner.
function Panels:shapeArrow(side)
    if self.arrowSide == side then
        return
    end
    self.arrowSide = side
    local direction = side == 'left' and -1 or 1
    local arrow = self.arrow
    arrow:clear()
    arrow:moveTo(0, 34)
    arrow:lineTo(0, 0)
    arrow:quadraticTo(0, -16, direction * 16, -16)
    arrow:lineTo(direction * 30, -16)
    arrow:moveTo(direction * 18, -30)
    arrow:lineTo(direction * 32, -16)
    arrow:lineTo(direction * 18, -2)
end

-- The navigation: the next turn with its distance and street, the map that moves with the car along the route, and the distance, the time and the arrival at the destination.
function Panels:drawNavigation(card, drive)
    Style.drawCard(card.x, card.y, card.width, card.height)
    local padding = Panels.padding
    local segment = drive:segment()
    self:shapeArrow(segment.turn)
    local headerY = card.y + padding + 46
    self.arrow:stroke(card.x + padding + 40, headerY + 4, self.arrowStroke)
    Style.drawText(Text.distance(drive:distanceToTurn()), card.x + padding + 104, headerY - 18, Style.type.value, Style.color.text, 0, 0.5, true)
    Style.drawText(drive:instruction(), card.x + padding + 104, headerY + 24, Style.type.small, Style.color.muted, 0, 0.5)

    local footerHeight = 76
    local map = self.mapClip
    map[1], map[2] = card.x + padding * 0.5, headerY + 70
    map[3], map[4] = card.width - padding, card.y + card.height - footerHeight - map[2]
    self:drawMap(drive)

    local footerY = card.y + card.height - footerHeight * 0.5
    local third = (card.width - padding * 2) / 3
    local left = card.x + padding
    Style.drawIcon(self.icons.flag, left + 18, footerY, 36, Style.color.text)
    Style.drawText(Text.distance(drive:distanceToDestination()), left + 46, footerY, Style.type.body, Style.color.text, 0, 0.5, true)
    Style.drawText(Text.duration(drive:minutesToDestination() * 60), left + third * 1.5, footerY, Style.type.body, Style.color.text, 0.5, 0.5, true)
    Style.drawText(Text.clock(drive.clock + drive:minutesToDestination() * 60), left + third * 3, footerY, Style.type.body, Style.color.text, 1, 0.5, true)
end

-- The map: the blocks of the city, a park and a river, the route ahead in cyan with the street of the turn named on it, and the car in the middle pointing the way it drives.
function Panels:drawMap(drive)
    local map = self.mapClip
    local x, y, width, height = map[1], map[2], map[3], map[4]
    graphics2d.pushMask(self.drawMapShape)
    graphics2d.drawRect(map, Style.color.map)

    local scale = width / Panels.mapSpan
    local centerX, centerY = x + width * 0.5, y + height * 0.62
    local block = Drive.block
    local originX, originY = centerX - drive.x * scale, centerY - drive.y * scale
    local firstColumn = math.floor((drive.x - (centerX - x) / scale) / block)
    local lastColumn = math.ceil((drive.x + (x + width - centerX) / scale) / block)
    local firstRow = math.floor((drive.y - (centerY - y) / scale) / block)
    local lastRow = math.ceil((drive.y + (y + height - centerY) / scale) / block)

    -- Every fifth block is a park, every seventh a pond, and the streets run between the blocks.
    for column = firstColumn, lastColumn do
        for row = firstRow, lastRow do
            local kind = (column * 7 + row * 3) % 11
            local color = kind == 0 and Style.color.park or (kind == 5 and Style.color.water or '#FF101B2E')
            local inset = 18 * scale
            Style.drawShape(originX + column * block * scale + inset, originY + row * block * scale + inset, block * scale - inset * 2, block * scale - inset * 2, 10, color)
        end
    end

    -- The route: from the car to the turn, then the streets after it, as the pairs of a flat list of fixed size whose count the stroke reads.
    local route = self.route
    route[1], route[2] = centerX, centerY
    local count = 1
    local px, py = drive.x, drive.y
    local hx, hy = drive.headingX, drive.headingY
    local remaining = drive:distanceToTurn()
    for index = 1, math.min(Panels.routeSegments, #drive.segments) do
        local length = index == 1 and remaining or drive.segments[index].length
        px, py = px + hx * length, py + hy * length
        count = count + 1
        route[count * 2 - 1], route[count * 2] = originX + px * scale, originY + py * scale
        if drive.segments[index].turn == 'right' then
            hx, hy = -hy, hx
        else
            hx, hy = hy, -hx
        end
    end
    self.routeOptions.count = count
    self.routeOptions.color = '#5522D3EE'
    self.routeOptions.width = 26
    graphics2d.drawStroke(route, self.routeOptions)
    self.routeOptions.color = Style.color.cyan
    self.routeOptions.width = 12
    graphics2d.drawStroke(route, self.routeOptions)

    -- The car: a blue disc with a white ring and an arrow toward its heading.
    local angle = math.atan(drive.headingY, drive.headingX) + math.pi * 0.5
    Style.drawShape(centerX - 30, centerY - 30, 60, 60, 0, '#FF3B82F6', self.markerOptions)
    self:drawHeading(centerX, centerY, angle)
    graphics2d.popMask()
end

-- A white arrow head turned toward the heading of the car.
function Panels:drawHeading(x, y, angle)
    local triangle = self.heading or {{0, 0}, {0, 0}, {0, 0}}
    self.heading = triangle
    local points = Panels.headingShape
    local cosine, sine = math.cos(angle), math.sin(angle)
    for index, point in ipairs(points) do
        triangle[index][1] = x + point[1] * cosine - point[2] * sine
        triangle[index][2] = y + point[1] * sine + point[2] * cosine
    end
    graphics2d.drawPolygon(triangle, '#FFFFFFFF')
end

Panels.headingShape = {{0, -16}, {12, 12}, {-12, 12}}

-- The trip computer: the distance, the average energy use and the time of the trip.
function Panels:drawTrip(card, drive)
    Style.drawCard(card.x, card.y, card.width, card.height)
    local padding = Panels.padding
    Style.drawText('Trip computer', card.x + padding, card.y + padding + 18, Style.type.body, Style.color.text, 0, 0.5, true)
    local third = (card.width - padding * 2) / 3
    local labelY = card.y + card.height * 0.5 + 6
    local valueY = labelY + 46
    local left = card.x + padding
    Panels.drawReading('Trip A', Text.format('%.1f km', drive.trip.distance, 0.1), left + third * 0.5, labelY, valueY)
    Panels.drawReading('Average', Text.format('%.1f kWh', drive:averageConsumption(), 0.1), left + third * 1.5, labelY, valueY)
    Panels.drawReading('Time', Text.duration(drive.trip.seconds), left + third * 2.5, labelY, valueY)
end

-- A reading of the trip computer: its name over its value.
function Panels.drawReading(label, value, x, labelY, valueY)
    Style.drawText(label, x, labelY, Style.type.small, Style.color.muted, 0.5, 0.5)
    Style.drawText(value, x, valueY, Style.type.small, Style.color.text, 0.5, 0.5, true)
end

-- The battery: its charge in percent with the bar that holds it, and the range.
function Panels:drawBattery(card, drive)
    Style.drawCard(card.x, card.y, card.width, card.height)
    local padding = Panels.padding
    local middle = card.y + card.height * 0.5
    Style.drawIcon(self.icons.bolt, card.x + padding + 26, middle, 64, Style.color.green)
    local charge = drive:charge()
    local left = card.x + padding + 72
    Style.drawText(Text.format('%.0f %%', charge * 100, 1), left, middle - 24, Style.type.value, Style.color.text, 0, 0.5, true)
    local range = card.width > Panels.wideCard
    local barWidth = range and card.width * 0.42 or card.width - padding * 2 - 72
    local barY = middle + 20
    Style.drawShape(left, barY, barWidth, 22, 11, '#FF16213A', Panels.barEdge)
    Style.drawShape(left + 3, barY + 3, (barWidth - 6) * charge, 16, 8, '#FFFFFFFF', Panels.batteryFill)
    local right = card.x + card.width - padding
    if range then
        Style.drawText('Range', right, middle - 22, Style.type.small, Style.color.muted, 1, 0.5)
        Style.drawText(Text.format('%.0f km', drive:range(), 1), right, middle + 20, Style.type.value, Style.color.text, 1, 0.5, true)
        return
    end
    Style.drawText(Text.format('%.0f km', drive:range(), 1), right, middle - 24, Style.type.label, Style.color.text, 1, 0.5, true)
end

-- The chart of the energy use: the last half a minute of consumption over a soft green area, with the value of the moment.
function Panels:drawConsumption(card, drive)
    Style.drawCard(card.x, card.y, card.width, card.height)
    local padding = Panels.padding
    Style.drawText('Energy use', card.x + padding, card.y + padding + 16, Style.type.body, Style.color.text, 0, 0.5, true)
    local valueWidth = card.width > Panels.wideCard and 170 or 0
    local chartX, chartY = card.x + padding, card.y + padding + 48
    local chartWidth, chartHeight = card.width - padding * 2 - valueWidth, card.height - padding * 2 - 48
    local history = drive.history
    local count = #history
    for index = 1, count do
        local value = m.clamp((history[index] + 10) / 50, 0, 1)
        local x = chartX + chartWidth * (index - 1) / (count - 1)
        local y = chartY + chartHeight * (1 - value)
        self.chartLine[index][1], self.chartLine[index][2] = x, y
        local top, bottom = self.chartArea[index * 2 - 1], self.chartArea[index * 2]
        top.x, top.y = x, y
        bottom.x, bottom.y = x, chartY + chartHeight
    end
    graphics2d.drawMesh(nil, self.chartArea, self.chartIndices)
    graphics2d.drawPolyline(self.chartLine, 5, Style.color.green, false)
    local right = card.x + card.width - padding
    if valueWidth > 0 then
        Style.drawText(Text.format('%.1f', drive.consumption, 0.1), right, chartY + chartHeight * 0.35, Style.type.value, Style.color.text, 1, 0.5, true)
        Style.drawText('kWh/100 km', right, chartY + chartHeight * 0.35 + 40, Style.type.small * 0.8, Style.color.muted, 1, 0.5)
        return
    end
    Style.drawText(Text.format('%.1f kWh/100 km', drive.consumption, 0.1), right, card.y + padding + 16, Style.type.small, Style.color.text, 1, 0.5, true)
end

-- The tyres: the car seen from above with a green mark on each tyre and the pressure of each next to it.
function Panels:drawTyres(card, drive)
    Style.drawCard(card.x, card.y, card.width, card.height)
    local padding = Panels.padding
    Style.drawIcon(self.icons.tyre, card.x + padding + 18, card.y + padding + 16, 36, Style.color.amber)
    Style.drawText('Tyre pressure', card.x + padding + 48, card.y + padding + 16, Style.type.body, Style.color.text, 0, 0.5, true)
    local top = card.y + padding + 48
    local height = card.y + card.height - padding - top
    local carHeight = height * 0.96
    local carWidth = carHeight * 0.5
    local centerX, centerY = card.x + card.width * 0.5, top + height * 0.5
    local options = self.topOptions
    options.width, options.height = carWidth, carHeight
    graphics2d.draw(self.images.top, centerX, centerY, options)
    for index, offset in ipairs(Panels.tyreOffsets) do
        local tyreX = centerX + offset[1] * carWidth * 0.43
        local tyreY = centerY + offset[2] * carHeight
        Style.drawShape(tyreX - 8, tyreY - 26, 16, 52, 8, '#FF22C55E', Panels.tyreGlow)
        local textX = centerX + offset[1] * (carWidth * 0.5 + 28)
        Style.drawText(Text.format('%.1f bar', drive.pressure[index], 0.1), textX, tyreY, Style.type.body, Style.color.text, offset[1] < 0 and 1 or 0, 0.5, true)
    end
end

-- The tiles of the assistants: eco mode, traction control, the brakes and the lane assist, lit while they are on.
Panels.assistants = {
    {icon = 'leaf', text = 'Eco', color = Style.color.green},
    {icon = 'traction', text = 'Traction', color = Style.color.cyan},
    {icon = 'abs', text = 'Brakes', color = Style.color.amber},
    {icon = 'lane', text = 'Lane assist', color = Style.color.green},
}

-- The tiles stand in one row on a wide card and two by two on a narrow one.
function Panels:drawAssistants(card)
    Style.drawCard(card.x, card.y, card.width, card.height)
    local padding = Panels.padding * 0.6
    local gap = 14
    local columns = card.width > Panels.wideCard and 4 or 2
    local rows = 4 / columns
    local width = (card.width - padding * 2 - gap * (columns - 1)) / columns
    local height = (card.height - padding * 2 - gap * (rows - 1)) / rows
    for index, tile in ipairs(Panels.assistants) do
        local column, row = (index - 1) % columns, (index - 1) // columns
        local x = card.x + padding + column * (width + gap)
        local y = card.y + padding + row * (height + gap)
        Style.drawShape(x, y, width, height, 22, '#FF111B33', Panels.tileEdge)
        local iconY = y + height * 0.4
        if tile.icon == 'abs' then
            Style.drawShape(x + width * 0.5 - 30, iconY - 30, 60, 60, 0, '#00000000', Panels.absRing)
            Style.drawText('ABS', x + width * 0.5, iconY, Style.type.small * 0.8, tile.color, 0.5, 0.5, true)
        else
            Style.drawIcon(self.icons[tile.icon], x + width * 0.5, iconY, math.min(56, height * 0.42), tile.color)
        end
        Style.drawText(tile.text, x + width * 0.5, y + height * 0.8, Style.type.small * 0.8, Style.color.text, 0.5, 0.5)
    end
end

-- Where the tyres sit on the car seen from above, as shares of its width and height from its middle, in the order of the pressures.
Panels.tyreOffsets = {{-1, -0.25}, {1, -0.25}, {-1, 0.26}, {1, 0.26}}
Panels.barEdge = {borderWidth = 2, borderColor = '#FF2C3E66'}
Panels.batteryFill = {texture = nil}
Panels.tyreGlow = {softness = 6}
Panels.tileEdge = {borderWidth = 2, borderColor = '#FF22324F'}
Panels.absRing = {kind = 'ellipse', borderWidth = 4, borderColor = Style.color.amber}

-- The texture of the battery exists once the style loaded it.
function Panels.prepare()
    Panels.batteryFill.texture = Style.battery
end

return Panels
