-- The texts of the panel, kept once they were made, so a frame that shows the same values as the one before builds no string.
local Text = {}

Text.numbers = {}
Text.cache = {}

-- A whole number as text.
function Text.number(value)
    local text = Text.numbers[value]
    if not text then
        text = tostring(value)
        Text.numbers[value] = text
    end
    return text
end

-- A text made from a pattern and a value rounded to the step of the readout, such as a tenth of a bar.
function Text.format(pattern, value, step)
    local rounded = math.floor(value / step + 0.5)
    local texts = Text.cache[pattern]
    if not texts then
        texts = {}
        Text.cache[pattern] = texts
    end
    local text = texts[rounded]
    if not text then
        text = string.format(pattern, rounded * step)
        texts[rounded] = text
    end
    return text
end

-- A time of the day from the seconds since midnight, such as 10:24.
function Text.clock(seconds)
    local minutes = math.floor(seconds / 60) % (24 * 60)
    local texts = Text.cache.clock
    if not texts then
        texts = {}
        Text.cache.clock = texts
    end
    local text = texts[minutes]
    if not text then
        text = string.format('%d:%02d', minutes // 60, minutes % 60)
        texts[minutes] = text
    end
    return text
end

-- A duration in hours and minutes, such as 1 h 36 min.
function Text.duration(seconds)
    local minutes = math.floor(seconds / 60)
    local texts = Text.cache.duration
    if not texts then
        texts = {}
        Text.cache.duration = texts
    end
    local text = texts[minutes]
    if not text then
        text = minutes >= 60 and string.format('%d h %d min', minutes // 60, minutes % 60) or string.format('%d min', minutes)
        texts[minutes] = text
    end
    return text
end

return Text
