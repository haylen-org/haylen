-- The middle of the dashboard: the speedometer, whose arc fills from cyan to orange with the speed around the gear and the speed, the road ahead seen from behind the car with its lanes, the cars of the other lanes and the lane assist, and the strip of the driver assistants under it.
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')

local Style = require('style')
local Text = require('text')

local Cluster = {}
Cluster.__index = Cluster

-- The arc runs clockwise from the bottom left to the bottom right, three quarters of a turn, up to 240 km/h.
Cluster.start = math.pi * 0.75
Cluster.sweep = math.pi * 1.5
Cluster.topSpeed = 240

-- The height of the eye over the road in meters, which sets the perspective, and the dashes of the lane lines.
Cluster.eye = 7
Cluster.dashPeriod = 12
Cluster.dashLength = 4.5
Cluster.horizon = 140

function Cluster.new(images)
    local self = setmetatable({images = images, ticks = {}, quad = {{0, 0}, {0, 0}, {0, 0}, {0, 0}}, triangle = {{0, 0}, {0, 0}, {0, 0}}}, Cluster)
    self.surface = {{x = 0, y = 0}, {x = 0, y = 0}, {x = 0, y = 0}, {x = 0, y = 0}}
    self.indices = {1, 2, 3, 1, 3, 4}
    self.carOptions = {pivotX = 0.5, pivotY = 1}
    return self
end

-- Places the ticks and the numbers of the scale around a gauge, once per layout.
function Cluster:layOut(gauge)
    local radius = gauge.radius
    local thickness = radius * 0.075
    self.thickness = thickness
    for index = 0, Cluster.topSpeed / 10 do
        local speed = index * 10
        local angle = Cluster.start + Cluster.sweep * speed / Cluster.topSpeed
        local major = speed % 20 == 0
        local outer = radius - thickness - radius * 0.035
        local inner = outer - (major and radius * 0.07 or radius * 0.04)
        local labelRadius = inner - radius * 0.09
        local cosine, sine = math.cos(angle), math.sin(angle)
        self.ticks[index + 1] = {
            speed = speed,
            major = major,
            x1 = gauge.x + cosine * outer, y1 = gauge.y + sine * outer,
            x2 = gauge.x + cosine * inner, y2 = gauge.y + sine * inner,
            labelX = gauge.x + cosine * labelRadius, labelY = gauge.y + sine * labelRadius,
            label = major and Text.number(speed) or nil,
        }
    end
end

function Cluster:drawGauge(gauge, drive)
    local x, y, radius = gauge.x, gauge.y, gauge.radius
    local thickness = self.thickness
    local speed = drive.speed * 3.6
    local fraction = m.clamp(speed / Cluster.topSpeed, 0, 1)

    -- The halo, the track and the arc of the speed, whose sector the face of the dial cuts into a ring.
    local halo = radius * 1.35
    Style.drawShape(x - halo, y - halo, halo * 2, halo * 2, 0, '#FFFFFFFF', Cluster.glowOptions)
    graphics2d.drawArc(x, y, radius - thickness * 0.5, thickness, Cluster.start, Cluster.start + Cluster.sweep, Style.color.track)
    if fraction > 0 then
        Cluster.arcOptions.sweep = Cluster.sweep * fraction
        Style.drawShape(x - radius, y - radius, radius * 2, radius * 2, 0, '#FFFFFFFF', Cluster.arcOptions)
    end
    local face = radius - thickness
    Style.drawShape(x - face, y - face, face * 2, face * 2, 0, Style.color.face, Cluster.faceOptions)

    -- A bright cap marks the end of the arc.
    local angle = Cluster.start + Cluster.sweep * fraction
    local capRadius = radius - thickness * 0.5
    local capX, capY = x + math.cos(angle) * capRadius, y + math.sin(angle) * capRadius
    Style.drawShape(capX - thickness * 1.6, capY - thickness * 1.6, thickness * 3.2, thickness * 3.2, 0, '#66FFFFFF', Cluster.capGlowOptions)
    Style.drawShape(capX - thickness * 0.55, capY - thickness * 0.55, thickness * 1.1, thickness * 1.1, 0, '#FFFFFFFF', Cluster.ellipseOptions)

    for _, tick in ipairs(self.ticks) do
        local passed = tick.speed <= speed
        graphics2d.drawLine(tick.x1, tick.y1, tick.x2, tick.y2, tick.major and 4 or 2, passed and Style.color.text or Style.color.faint)
        if tick.label then
            Style.drawText(tick.label, tick.labelX, tick.labelY, radius * 0.075, passed and Style.color.text or Style.color.muted, 0.5, 0.5)
        end
    end

    -- The gear in its pill, the speed and its unit.
    local pillWidth, pillHeight = radius * 0.24, radius * 0.17
    local pillY = y - radius * 0.58
    Style.drawShape(x - pillWidth * 0.5, pillY, pillWidth, pillHeight, pillHeight * 0.5, '#FF111B33', Cluster.pillOptions)
    Style.drawText('D', x, pillY + pillHeight * 0.5, radius * 0.12, Style.color.text, 0.5, 0.5, true)
    Style.drawText(Text.number(math.floor(speed + 0.5)), x, y - radius * 0.18, radius * 0.38, Style.color.text, 0.5, 0.5, true)
    Style.drawText('km/h', x, y + radius * 0.08, radius * 0.08, Style.color.muted, 0.5, 0.5)
end

-- The screen position of a point on the road `distance` meters ahead and `lane` lanes from the middle of the own lane.
function Cluster:project(distance, lane)
    local scale = Cluster.eye / (distance + Cluster.eye)
    local y = self.horizonY + (self.bottomY - self.horizonY) * scale
    return self.centerX + lane * self.laneWidth * scale, y, scale
end

-- Fills a quad of the road between two distances and two lane positions.
function Cluster:fillLane(near, far, laneLeft, laneRight, color)
    local quad = self.quad
    quad[1][1], quad[1][2] = self:project(far, laneLeft)
    quad[2][1], quad[2][2] = self:project(far, laneRight)
    quad[3][1], quad[3][2] = self:project(near, laneRight)
    quad[4][1], quad[4][2] = self:project(near, laneLeft)
    graphics2d.drawPolygon(quad, color)
end

-- Fills a quad of the road with a color that fades out toward the horizon.
function Cluster:drawFading(laneLeft, laneRight, nearColor, farColor)
    local surface = self.surface
    surface[1].x, surface[1].y = self:project(Cluster.horizon, laneLeft)
    surface[2].x, surface[2].y = self:project(Cluster.horizon, laneRight)
    surface[3].x, surface[3].y = self:project(0, laneRight)
    surface[4].x, surface[4].y = self:project(0, laneLeft)
    surface[1].color, surface[2].color = farColor, farColor
    surface[3].color, surface[4].color = nearColor, nearColor
    graphics2d.drawMesh(nil, surface, self.indices)
end

function Cluster:drawRoad(gauge, road, drive)
    local radius = gauge.radius
    self.centerX = gauge.x
    self.horizonY = gauge.y + radius * 0.2
    self.bottomY = road.y + road.height
    self.laneWidth = math.min(road.width * 0.3, radius * 0.44)

    -- The road and the own lane, which the lane assist lights in green.
    self:drawFading(-1.5, 1.5, '#FF101A30', '#000C1426')
    self:drawFading(-0.5, 0.5, '#5522C55E', '#0022C55E')

    -- The edges of the road and the dashes between the lanes, which run toward the car with its speed.
    self:fillLane(0, Cluster.horizon, -1.53, -1.47, '#AA9FB0CC')
    self:fillLane(0, Cluster.horizon, 1.47, 1.53, '#AA9FB0CC')
    local phase = drive.travelled % Cluster.dashPeriod
    for index = 0, 12 do
        local near = index * Cluster.dashPeriod - phase
        local far = near + Cluster.dashLength
        if far > 0 then
            near = math.max(near, 0)
            self:fillLane(near, far, -0.53, -0.47, '#CC22C55E')
            self:fillLane(near, far, 0.47, 0.53, '#CC22C55E')
        end
    end

    -- The cars of the other lanes, far ones first, and the own car in front of the view.
    local options = self.carOptions
    for index = #drive.traffic, 1, -1 do
        local car = drive.traffic[index]
        local x, y, scale = self:project(car.distance, car.lane)
        local width = self.laneWidth * scale * 0.82
        options.width, options.height = width, width * 0.75
        options.color = '#FF8FB4FF'
        graphics2d.draw(self.images.rear, x, y, options)
    end
    local x, y, scale = self:project(4, 0)
    local width = self.laneWidth * scale * 0.92
    options.width, options.height = width, width * 0.75
    options.color = '#FFFFFFFF'
    graphics2d.draw(self.images.rear, x, y, options)

    -- The tail lights glow while the car brakes, and the corner of the turn blinks with the signal.
    local lightY = y - width * 0.75 * 0.55
    if drive.acceleration < -0.6 then
        Style.drawShape(x - width * 0.42, lightY - width * 0.05, width * 0.84, width * 0.1, width * 0.05, '#AAFF3B4E', Cluster.softOptions)
    end
    local lit, side = drive:signal()
    if lit then
        local cornerX = side == 'left' and x - width * 0.4 or x + width * 0.4
        Style.drawShape(cornerX - width * 0.08, lightY - width * 0.06, width * 0.16, width * 0.12, 0, '#DDF5B83D', Cluster.signalOptions)
    end
end

-- Draws a turn arrow, pointing left or right, lit while the signal blinks on that side.
function Cluster:drawSignal(x, y, size, side, lit)
    local triangle = self.triangle
    local direction = side == 'left' and -1 or 1
    triangle[1][1], triangle[1][2] = x + direction * size * 0.5, y
    triangle[2][1], triangle[2][2] = x - direction * size * 0.4, y - size * 0.45
    triangle[3][1], triangle[3][2] = x - direction * size * 0.4, y + size * 0.45
    graphics2d.drawPolygon(triangle, lit and Style.color.amber or Style.color.faint)
end

-- The strip under the gauge: the signals at its ends, the lane assist, the set speed of the adaptive cruise and the power of the motor or the energy that braking recovers.
function Cluster:drawStrip(strip, drive, icons)
    local x, y, width, height = strip.x, strip.y, strip.width, strip.height
    Style.drawShape(x, y, width, height, height * 0.5, Style.color.panel, Style.cardEdge)
    local middle = y + height * 0.5
    local lit, side = drive:signal()
    self:drawSignal(x + height * 0.6, middle, height * 0.36, 'left', lit and side == 'left')
    self:drawSignal(x + width - height * 0.6, middle, height * 0.36, 'right', lit and side == 'right')

    -- The lane assist, the cruise and the power share the room between the signals in thirds.
    local inner = width - height * 2.4
    local left = x + height * 1.2
    Style.drawIcon(icons.lane, left + inner * 0.14, middle, height * 0.5, Style.color.green)

    local cruiseX = left + inner * 0.47
    Style.drawIcon(icons.cruise, cruiseX - height * 0.36, middle, height * 0.46, Style.color.cyan)
    Style.drawText(Text.number(Cluster.cruiseSpeed), cruiseX - height * 0.06, middle - 2, Style.type.body, Style.color.cyan, 0, 0.5, true)

    local power = drive.power
    local powerX = left + inner * 0.84
    Style.drawText(Text.power(power), powerX, middle - height * 0.15, Style.type.body, power < 0 and Style.color.green or Style.color.text, 0.5, 0.5, true)
    Style.drawText(power < 0 and 'Regen' or 'Power', powerX, middle + height * 0.24, Style.type.small * 0.8, Style.color.muted, 0.5, 0.5)
end

Cluster.cruiseSpeed = 100
Cluster.glowOptions = {kind = 'ellipse', texture = nil}
Cluster.arcOptions = {kind = 'ellipse', texture = nil, startAngle = Cluster.start, sweep = 0}
Cluster.faceOptions = {kind = 'ellipse', borderWidth = 2, borderColor = '#FF1A2644'}
Cluster.ellipseOptions = {kind = 'ellipse'}
Cluster.capGlowOptions = {kind = 'ellipse', softness = 24}
Cluster.pillOptions = {borderWidth = 2, borderColor = '#FF2C3E66'}
Cluster.softOptions = {softness = 18}
Cluster.signalOptions = {kind = 'ellipse', softness = 16}

-- The textures of the options exist once the style loaded them.
function Cluster.prepare()
    Cluster.glowOptions.texture = Style.glow
    Cluster.arcOptions.texture = Style.arc
end

return Cluster
