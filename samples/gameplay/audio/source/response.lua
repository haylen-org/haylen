-- The frequency response of the filters of `haylen.audio`, from the formulas of the Audio EQ Cookbook of Robert Bristow-Johnson that the engine uses, so the effects test can draw the curve a filter applies.
local response = {}

local kSampleRate = 48000

-- Returns the coefficients `b0`, `b1`, `b2`, `a0`, `a1` and `a2` of a filter kind at a cutoff, `q` and gain.
local function coefficients(kind, cutoff, q, gain)
    local w0 = 2 * math.pi * math.min(cutoff, kSampleRate * 0.49) / kSampleRate
    local cos, alpha = math.cos(w0), math.sin(w0) / (2 * q)
    local a = 10 ^ (gain / 40)
    local root = 2 * math.sqrt(a) * alpha
    if kind == 'lowpass' then
        return (1 - cos) / 2, 1 - cos, (1 - cos) / 2, 1 + alpha, -2 * cos, 1 - alpha
    elseif kind == 'highpass' then
        return (1 + cos) / 2, -(1 + cos), (1 + cos) / 2, 1 + alpha, -2 * cos, 1 - alpha
    elseif kind == 'bandpass' then
        return alpha, 0, -alpha, 1 + alpha, -2 * cos, 1 - alpha
    elseif kind == 'notch' then
        return 1, -2 * cos, 1, 1 + alpha, -2 * cos, 1 - alpha
    elseif kind == 'peak' then
        return 1 + alpha * a, -2 * cos, 1 - alpha * a, 1 + alpha / a, -2 * cos, 1 - alpha / a
    elseif kind == 'lowShelf' then
        return a * ((a + 1) - (a - 1) * cos + root), 2 * a * ((a - 1) - (a + 1) * cos), a * ((a + 1) - (a - 1) * cos - root),
            (a + 1) + (a - 1) * cos + root, -2 * ((a - 1) + (a + 1) * cos), (a + 1) + (a - 1) * cos - root
    end
    return a * ((a + 1) + (a - 1) * cos + root), -2 * a * ((a - 1) + (a + 1) * cos), a * ((a + 1) + (a - 1) * cos - root),
        (a + 1) - (a - 1) * cos + root, 2 * ((a - 1) - (a + 1) * cos), (a + 1) - (a - 1) * cos - root
end

-- Returns the gain in decibels that a filter applies at a frequency in hertz.
function response.decibels(kind, cutoff, q, gain, frequency)
    local b0, b1, b2, a0, a1, a2 = coefficients(kind, cutoff, q, gain)
    local w = 2 * math.pi * frequency / kSampleRate
    local cos1, sin1, cos2, sin2 = math.cos(w), math.sin(w), math.cos(2 * w), math.sin(2 * w)
    local top = (b0 + b1 * cos1 + b2 * cos2) ^ 2 + (b1 * sin1 + b2 * sin2) ^ 2
    local bottom = (a0 + a1 * cos1 + a2 * cos2) ^ 2 + (a1 * sin1 + a2 * sin2) ^ 2
    return 10 * math.log(math.max(top / bottom, 1e-12), 10)
end

-- Maps a slider position from 0 to 1 to a frequency from 20 Hz to 20 kHz, so every octave takes the same room.
function response.frequency(position)
    return 20 * 1000 ^ position
end

function response.position(frequency)
    return math.log(frequency / 20, 1000)
end

return response
