-- The island clock: dawn, day, dusk and night in a loop, where a new day begins with each dawn. The ambient light eases from night through the dawn color into day, and back through the dusk color.
local m = require('haylen.math')

local dayNight = {}
dayNight.__index = dayNight

local phases = {'dawn', 'day', 'dusk', 'night'}

-- Blends from one color to another through a middle color, reaching the middle halfway through the phase.
local function blend(from, middle, to, progress)
    if progress < 0.5 then
        return from:lerp(middle, m.smoothstep(0, 0.5, progress))
    end
    return middle:lerp(to, m.smoothstep(0.5, 1, progress))
end

-- The argument `lengths` holds the seconds of each phase, `colors` the ambient light of each phase, and `time` the second of the cycle to start at.
function dayNight.new(lengths, colors, time)
    local self = setmetatable({lengths = lengths, colors = {}, day = 1}, dayNight)
    for _, phase in ipairs(phases) do
        self.colors[phase] = m.color(colors[phase])
    end
    self.cycleLength = lengths.dawn + lengths.day + lengths.dusk + lengths.night
    self:setTime(time)
    return self
end

function dayNight:phaseStart(phase)
    local start = 0
    for _, current in ipairs(phases) do
        if current == phase then
            return start
        end
        start = start + self.lengths[current]
    end
end

-- Updates the phase, its progress and the ambient light after the clock moved.
function dayNight:refresh()
    for _, phase in ipairs(phases) do
        if self.time >= self:phaseStart(phase) then
            self.phase = phase
        end
    end
    self.progress = m.saturate((self.time - self:phaseStart(self.phase)) / self.lengths[self.phase])

    local colors = self.colors
    if self.phase == 'dawn' then
        self.ambient = blend(colors.night, colors.dawn, colors.day, self.progress)
    elseif self.phase == 'dusk' then
        self.ambient = blend(colors.day, colors.dusk, colors.night, self.progress)
    else
        self.ambient = colors[self.phase]
    end
end

-- Scales a light color, channel by channel, by the light the ambient color leaves missing, so lights fill the island up to its unlit colors in the dark and fade out as the day brightens.
function dayNight:fill(color)
    local ambient = self.ambient
    return m.color(color.r * (1 - ambient.r), color.g * (1 - ambient.g), color.b * (1 - ambient.b), color.a)
end

-- Jumps to a second of the cycle without reporting the phases in between.
function dayNight:setTime(seconds)
    self.time = math.fmod(math.fmod(seconds, self.cycleLength) + self.cycleLength, self.cycleLength)
    self:refresh()
end

-- Advances the clock and reports every phase and day it passes, in order, through `onPhase(phase, day)` and `onDay(day)`.
function dayNight:update(dt)
    local remaining = dt
    while remaining > 0 do
        local left = self:phaseStart(self.phase) + self.lengths[self.phase] - self.time
        if remaining < left then
            self.time = self.time + remaining
            self:refresh()
            return
        end

        -- Crossing a boundary lands exactly on the next phase, so every phase gets reported even on long frames.
        remaining = remaining - left
        self.time = self.time + left
        if self.time >= self.cycleLength then
            self.time = 0
        end
        self:refresh()
        if self.phase == 'dawn' then
            self.day = self.day + 1
            if self.onDay then
                self.onDay(self.day)
            end
        end
        if self.onPhase then
            self.onPhase(self.phase, self.day)
        end
    end
end

return dayNight
