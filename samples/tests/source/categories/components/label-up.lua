-- Label up: numbers and words that pop out of what was hit, healed or collected, rise and fade, such as "-12" in red over an enemy, "+5" in green over the hero or "Critical!" in large gold.
-- The module needs only the engine, so it copies as it is into another project. Create one with `LabelUp.new(options)`, call `labels:spawn(x, y, text, kind)` where something happens, and `labels:update(dt)` and `labels:draw()` every frame in the world canvas.
-- Labels are records of a pool that the module reuses, and each keeps its own style and colors that it changes in place, so a frame that spawns nothing allocates nothing.
local graphics2d = require('haylen.graphics2d')

local LabelUp = {}
LabelUp.__index = LabelUp

-- The kinds of label with their look: the fill and outline colors, the size relative to the size of the labels, how far it rises relative to the rise, and the pop of its scale.
LabelUp.kinds = {
    damage = {fill = {1, 0.32, 0.27}, outline = {0.24, 0.04, 0.06}, size = 1, rise = 1, pop = 1.35},
    critical = {fill = {1, 0.84, 0.25}, outline = {0.42, 0.13, 0.02}, size = 1.6, rise = 1.25, pop = 1.7},
    heal = {fill = {0.42, 0.93, 0.45}, outline = {0.03, 0.22, 0.08}, size = 1, rise = 1, pop = 1.25},
    gold = {fill = {1, 0.86, 0.36}, outline = {0.35, 0.2, 0.02}, size = 0.9, rise = 0.9, pop = 1.2},
    miss = {fill = {0.85, 0.88, 0.95}, outline = {0.13, 0.15, 0.22}, size = 0.85, rise = 0.7, pop = 1},
}

-- Options: `font` (the default font when nil), `size` (32), `rise` in units (90), `life` in seconds (0.9), `spread` of the random sideways drift in units (24) and `layer` (10).
function LabelUp.new(options)
    options = options or {}
    local self = setmetatable({
        font = options.font,
        size = options.size or 32,
        rise = options.rise or 90,
        life = options.life or 0.9,
        spread = options.spread or 24,
        layer = options.layer or 10,
        records = {},
        count = 0,
        random = 0x2545F491,
    }, LabelUp)
    return self
end

-- A small generator of its own, so the drift of the labels never touches the random state of the game.
function LabelUp:roll()
    self.random = (self.random * 1103515245 + 12345) % 2147483648
    return self.random / 2147483648
end

-- Shows `text` rising from `x`, `y` in the look of `kind`, which is one of `LabelUp.kinds` or a table of the same fields.
function LabelUp:spawn(x, y, text, kind)
    local look = type(kind) == 'table' and kind or LabelUp.kinds[kind or 'damage']
    if not look then
        error('The kind of label "' .. tostring(kind) .. '" is unknown. It is "damage", "critical", "heal", "gold", "miss" or a table of the same fields.', 2)
    end

    self.count = self.count + 1
    local record = self.records[self.count]
    if not record then
        record = {fill = {0, 0, 0, 1}, outline = {0, 0, 0, 1}, scale = {1, 1}}
        record.style = {size = 0, color = record.fill, outlineWidth = 0, outlineColor = record.outline, anchor = {0.5, 1}, scale = record.scale, layer = 0}
        self.records[self.count] = record
    end

    -- Labels that start close together stack upward, so a quick series of hits stays readable.
    local stacked = 0
    for index = 1, self.count - 1 do
        local other = self.records[index]
        if other.age < 0.15 and math.abs(other.x - x) < 40 and math.abs(other.startY - y) < 40 then
            stacked = stacked + 1
        end
    end

    record.text, record.look, record.age = text, look, 0
    record.x, record.startY = x + (self:roll() - 0.5) * self.spread, y - stacked * self.size * 0.8
    record.drift = (self:roll() - 0.5) * self.spread
    for channel = 1, 3 do
        record.fill[channel], record.outline[channel] = look.fill[channel], look.outline[channel]
    end
    record.style.size = self.size * look.size
    record.style.outlineWidth = math.max(2, self.size * look.size * 0.12)
    record.style.layer = self.layer
end

function LabelUp:update(dt)
    local index = 1
    while index <= self.count do
        local record = self.records[index]
        record.age = record.age + dt
        if record.age >= self.life then
            -- The last live record takes the place of the one that ended, so the pool stays packed.
            self.records[index], self.records[self.count] = self.records[self.count], record
            self.count = self.count - 1
        else
            index = index + 1
        end
    end
end

function LabelUp:draw()
    for index = 1, self.count do
        local record = self.records[index]
        local t = record.age / self.life

        -- The label pops past its size, settles, rises fast and slows down, and fades in the last third of its life.
        local pop = t < 0.12 and 1 + (record.look.pop - 1) * (t / 0.12) or 1 + (record.look.pop - 1) * math.max(0, 1 - (t - 0.12) / 0.18)
        local rise = 1 - (1 - t) * (1 - t)
        local alpha = t < 0.66 and 1 or 1 - (t - 0.66) / 0.34
        record.scale[1], record.scale[2] = pop, pop
        record.fill[4], record.outline[4] = alpha, alpha
        graphics2d.drawText(self.font, record.text, record.x + record.drift * rise, record.startY - self.rise * record.look.rise * rise, record.style)
    end
end

-- Removes every label, such as when the scene that shows them restarts.
function LabelUp:clear()
    self.count = 0
end

return LabelUp
