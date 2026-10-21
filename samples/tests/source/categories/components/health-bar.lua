-- Health bar: the bar over a character or in a corner of the screen, whose fill eases to the health, while a light trail keeps the health just lost and drains after it a moment later, so every hit reads at a glance.
-- The module needs only the engine, so it copies as it is into another project. Create one with `HealthBar.new(maximum, options)`, set `bar:setHealth(value)` or call `bar:damage(amount)` and `bar:heal(amount)`, and call `bar:update(dt)` and `bar:draw(x, y)` every frame, where `x`, `y` is the center of the bar.
-- The fill turns from green to yellow to red as the health drops, ticks mark every `tick` points, an optional shield draws over the end of the fill, the bar flashes white on a hit, and with `hideWhenFull` it shows only after a change and fades out once it rests full.
local graphics2d = require('haylen.graphics2d')

local HealthBar = {}
HealthBar.__index = HealthBar

-- The width of the border around the groove of the fills.
HealthBar.border = 2

HealthBar.colors = {
    back = {0.08, 0.09, 0.14, 0.85},
    border = {0.04, 0.05, 0.08, 1},
    trail = {1, 0.93, 0.78, 1},
    shield = {0.55, 0.82, 1, 1},
    full = {0.36, 0.86, 0.38},
    half = {1, 0.8, 0.22},
    low = {0.95, 0.27, 0.24},
}

-- Options: `width` (120) and `height` (16) in units, `tick` (25, or 0 for no ticks), `shield` (0), `hideWhenFull` (false), `layer` (10), and `colors`, a table that replaces any of `HealthBar.colors`.
function HealthBar.new(maximum, options)
    if type(maximum) ~= 'number' or maximum <= 0 then
        error('A health bar needs a maximum health above 0.', 2)
    end
    options = options or {}
    local colors = setmetatable(options.colors or {}, {__index = HealthBar.colors})
    local self = setmetatable({
        maximum = maximum,
        health = maximum,
        shown = maximum,
        trail = maximum,
        shield = options.shield or 0,
        width = options.width or 120,
        height = options.height or 16,
        tick = options.tick or 25,
        hideWhenFull = options.hideWhenFull or false,
        layer = options.layer or 10,
        colors = colors,
        hold = 0,
        flash = 0,
        visible = options.hideWhenFull and 0 or 1,
        -- A bar that hides while full starts hidden, as if it had rested full for a while.
        idle = options.hideWhenFull and 2 or 0,
        -- The tables that the draw fills in place, so drawing allocates nothing.
        rect = {0, 0, 0, 0},
        fill = {0, 0, 0, 1},
        tinted = {{0, 0, 0, 0}, {0, 0, 0, 0}},
        order = {layer = 0},
        shape = {radius = 0, color = nil, borderWidth = 0, borderColor = nil, layer = 0},
        groove = {0, 0, 0, 0},
        grooveShape = {radius = 0, color = '#FFFFFFFF'},
    }, HealthBar)
    -- The groove inside the border masks the fills, so a fill keeps the curve of the track at its start however short it is.
    self.drawGroove = function()
        graphics2d.drawShape(self.groove, self.grooveShape)
    end
    return self
end

function HealthBar:setHealth(value)
    value = math.max(0, math.min(self.maximum, value))
    if value < self.health then
        -- The trail waits a moment at the health before the hit, then drains after the fill.
        self.trail = math.max(self.trail, self.health)
        self.hold = 0.45
        self.flash = 1
    end
    self.health = value
    self.idle = 0
end

-- Takes the damage from the shield first and the rest from the health, and answers how much health it took.
function HealthBar:damage(amount)
    local absorbed = math.min(self.shield, amount)
    self.shield = self.shield - absorbed
    local before = self.health
    self:setHealth(self.health - (amount - absorbed))
    if absorbed > 0 then
        self.flash = 1
        self.idle = 0
    end
    return before - self.health
end

function HealthBar:heal(amount)
    self:setHealth(self.health + amount)
end

function HealthBar:setShield(value)
    self.shield = math.max(0, value)
    self.idle = 0
end

function HealthBar:isEmpty()
    return self.health <= 0
end

function HealthBar:update(dt)
    -- The fill eases to the health in about a tenth of a second, and a heal grows the trail with it.
    self.shown = self.shown + (self.health - self.shown) * math.min(1, dt * 14)
    if self.hold > 0 then
        self.hold = self.hold - dt
    else
        self.trail = math.max(self.shown, self.trail - self.maximum * dt * 0.9)
    end
    self.flash = math.max(0, self.flash - dt * 5)

    self.idle = self.idle + dt
    local wanted = (not self.hideWhenFull or self.health < self.maximum or self.idle < 1.5) and 1 or 0
    self.visible = self.visible + (wanted - self.visible) * math.min(1, dt * 8)
end

-- Turns the fill from green through yellow to red as the share of health drops.
function HealthBar:fillColor(share)
    local from, to, t
    if share > 0.5 then
        from, to, t = self.colors.half, self.colors.full, (share - 0.5) * 2
    else
        from, to, t = self.colors.low, self.colors.half, share * 2
    end
    for channel = 1, 3 do
        local value = from[channel] + (to[channel] - from[channel]) * t
        self.fill[channel] = value + (1 - value) * self.flash * 0.8
    end
    self.fill[4] = self.visible
    return self.fill
end

-- Copies a color into one of two slots with its alpha multiplied by how visible the bar is, so a fill and a border can both fade in one draw.
function HealthBar:faded(color, slot)
    local tinted = self.tinted[slot or 1]
    tinted[1], tinted[2], tinted[3], tinted[4] = color[1], color[2], color[3], (color[4] or 1) * self.visible
    return tinted
end

function HealthBar:box(left, top, width, height, color, borderWidth, borderColor, layer)
    local rect, shape = self.rect, self.shape
    rect[1], rect[2], rect[3], rect[4] = left, top, width, height
    shape.radius, shape.color, shape.borderWidth, shape.borderColor, shape.layer = height / 2, color, borderWidth, borderColor, layer
    graphics2d.drawShape(rect, shape)
end

-- A fill is a pill whose round end lands where the fill ends and which reaches past the start of the groove when it is shorter than a pill, where the mask of the groove cuts it.
function HealthBar:fillBox(left, top, width, height, color, layer)
    local span = math.max(width, height)
    self:box(left + width - span, top, span, height, color, 0, nil, layer)
end

-- Draws the bar centered on `x`, `y`.
function HealthBar:draw(x, y)
    if self.visible < 0.01 then
        return
    end

    local width, height = self.width, self.height
    local left, top = x - width / 2, y - height / 2
    local layer = self.layer

    -- The fills sit in the groove inside the border, whose round ends follow the ends of the track.
    local border = HealthBar.border
    local innerLeft, innerTop = left + border, top + border
    local inner, innerHeight = width - border * 2, height - border * 2
    self:box(left, top, width, height, self:faded(self.colors.back, 1), border, self:faded(self.colors.border, 2), layer)
    local groove = self.groove
    groove[1], groove[2], groove[3], groove[4] = innerLeft, innerTop, inner, innerHeight
    self.grooveShape.radius = innerHeight / 2
    self.order.layer = layer + 1
    graphics2d.pushMask(self.drawGroove, self.order)
    if self.trail > self.shown then
        self:fillBox(innerLeft, innerTop, inner * self.trail / self.maximum, innerHeight, self:faded(self.colors.trail), 1)
    end
    if self.shown > 0.5 then
        self:fillBox(innerLeft, innerTop, inner * self.shown / self.maximum, innerHeight, self:fillColor(self.shown / self.maximum), 2)
    end
    if self.shield > 0 then
        local share = math.min(self.shield, self.maximum) / self.maximum
        self:fillBox(innerLeft, innerTop, inner * share, innerHeight, self:faded(self.colors.shield), 3)
    end
    graphics2d.popMask()

    if self.tick > 0 then
        local tick = self:faded(self.colors.border, 2)
        self.order.layer = layer + 4
        for value = self.tick, self.maximum - 1, self.tick do
            local tickX = innerLeft + inner * value / self.maximum
            graphics2d.drawLine(tickX, innerTop + 1, tickX, innerTop + innerHeight - 1, 1.5, tick, self.order)
        end
    end
end

return HealthBar
