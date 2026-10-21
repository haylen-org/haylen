-- The synthesizer that makes every song of the app from its seed: a chord progression on a pad, a bass, a melody and drums, in the tempo, scale and sound of its genre, rendered into 16-bit samples inside a job of haylen.jobs, so a song never holds up a frame while it is made.
local jobs = require('haylen.jobs')

local Random = require('random')

local Synth = {}

Synth.rate = 22050
Synth.bars = 12
Synth.chunk = 1024
Synth.tableSize = 4096
Synth.sine = {}
for index = 0, Synth.tableSize - 1 do
    Synth.sine[index] = math.sin(index / Synth.tableSize * math.pi * 2)
end

Synth.scales = {
    major = {0, 2, 4, 5, 7, 9, 11},
    minor = {0, 2, 3, 5, 7, 8, 10},
    dorian = {0, 2, 3, 5, 7, 9, 10},
    pentatonic = {0, 2, 4, 7, 9, 12, 14},
}

-- The sound of each genre: its tempos, its scale, the loudness of its drums, hats and melody, and how short its notes are.
Synth.styles = {
    pop = {tempo = {104, 124}, scale = 'major', drums = 1, hats = 1, lead = 1, decay = 3},
    electronic = {tempo = {118, 128}, scale = 'minor', drums = 1.1, hats = 1.4, lead = 0.9, decay = 5},
    lofi = {tempo = {72, 86}, scale = 'dorian', drums = 0.7, hats = 0.6, lead = 0.8, decay = 2},
    acoustic = {tempo = {88, 104}, scale = 'major', drums = 0, hats = 0, lead = 1.2, decay = 4},
    ambient = {tempo = {64, 76}, scale = 'pentatonic', drums = 0, hats = 0.2, lead = 0.7, decay = 1},
    rock = {tempo = {112, 132}, scale = 'minor', drums = 1.2, hats = 1, lead = 1, decay = 2.5},
    jazz = {tempo = {90, 112}, scale = 'dorian', drums = 0.6, hats = 1.2, lead = 1.1, decay = 3.5},
    funk = {tempo = {100, 116}, scale = 'dorian', drums = 1, hats = 1.3, lead = 1, decay = 4},
}

Synth.progressions = {{1, 5, 6, 4}, {1, 6, 4, 5}, {6, 4, 1, 5}, {1, 4, 6, 5}, {2, 5, 1, 6}}

function Synth.frequency(midi)
    return 440 * 2 ^ ((midi - 69) / 12)
end

-- Plans the song: its tempo, its chords and the notes of its melody on every eighth.
function Synth.plan(seed, style)
    local random = Random(seed)
    local scale = Synth.scales[style.scale]
    local root = 45 + random:integer(0, 11)
    local progression = random:pick(Synth.progressions)
    local plan = {beat = 60 / random:integer(style.tempo[1], style.tempo[2]), chords = {}, melody = {}}
    for bar = 1, 4 do
        local degree = progression[bar]
        local notes = {}
        for voice = 0, 2 do
            local step = degree - 1 + voice * 2
            notes[#notes + 1] = root + 12 + scale[step % #scale + 1] + 12 * (step // #scale)
        end
        plan.chords[bar] = {bass = root - 12 + scale[(degree - 1) % #scale + 1], notes = notes, frequencies = {Synth.frequency(notes[1]), Synth.frequency(notes[2]), Synth.frequency(notes[3])}}
    end
    -- The melody repeats its first two bars with a change in the second half, the way a hook comes back.
    local motif = {}
    for eighth = 1, 16 do
        motif[eighth] = random:chance(0.68) and random:integer(0, 6) or false
    end
    for eighth = 1, Synth.bars * 8 do
        local index = (eighth - 1) % 16 + 1
        local note = motif[index]
        if eighth > Synth.bars * 4 and random:chance(0.25) then
            note = random:integer(0, 6)
        end
        local chord = plan.chords[((eighth - 1) // 8) % 4 + 1]
        plan.melody[eighth] = note and Synth.frequency(chord.notes[1] + 12 + scale[note + 1] - scale[1]) or false
    end
    plan.length = plan.beat * 4 * Synth.bars
    return plan
end

-- Fills `levels` with how loud each voice of a song sounds at a time, from 0 to 1, read from the same plan and envelopes that render it, so a visualizer follows the music without listening to it: the kick, the snare, the hats, the bass, the melody and the pad.
function Synth.levels(plan, style, time, levels)
    local beat = plan.beat
    local eighth = beat / 2
    local beats = time / beat
    local whole = math.floor(beats)
    local inBeat = (beats - whole) * beat
    local step = math.floor(time / eighth) + 1
    local inEighth = time - (step - 1) * eighth
    local inBar = time - math.floor(beats / 4) * 4 * beat
    local onKick = whole % 4 == 0 or whole % 4 == 2
    levels.kick = onKick and math.exp(-inBeat * 9) * style.drums or 0
    levels.snare = onKick and 0 or math.exp(-inBeat * 12) * style.drums * 0.8
    levels.hats = math.exp(-inEighth * 30) * math.min(1, style.hats)
    levels.bass = math.exp(-inBeat * 3)
    levels.lead = plan.melody[step] and math.exp(-inEighth * style.decay * 0.6) * math.min(1, style.lead) or 0
    levels.pad = math.max(0, math.min(1, inBar / 0.4) * (1 - inBar / (8 * beat)))
    return levels
end

-- Renders a song into the bytes of 16-bit mono samples. It runs inside a job and pauses at checkpoints.
function Synth.render(seed, styleName)
    local style = Synth.styles[styleName]
    local plan = Synth.plan(seed, style)
    local rate, sine, size = Synth.rate, Synth.sine, Synth.tableSize
    local total = math.floor(plan.length * rate)
    local beat = plan.beat
    local eighth = beat / 2
    local exp, floor = math.exp, math.floor
    local noise = seed * 7919 % 2147483647
    local parts = {}
    local buffer = {}
    local format = '<' .. string.rep('h', Synth.chunk)
    local position = 0
    while position < total do
        local count = math.min(Synth.chunk, total - position)
        for index = 1, count do
            local time = (position + index - 1) / rate
            local beats = time / beat
            local bar = floor(beats / 4)
            local chord = plan.chords[bar % 4 + 1]
            local inBeat = (beats - floor(beats)) * beat
            local inBar = time - bar * 4 * beat
            local sample = 0
            -- The pad swells at the start of every bar and fades toward its end.
            local pad = math.min(1, inBar / 0.4) * (1 - inBar / (8 * beat)) * 0.07
            local frequencies = chord.frequencies
            for voice = 1, 3 do
                sample = sample + sine[floor(time * frequencies[voice] % 1 * size)] * pad
            end
            local bassFrequency = 440 * 2 ^ ((chord.bass - 69) / 12)
            local bassPhase = time * bassFrequency % 1
            sample = sample + (sine[floor(bassPhase * size)] + 0.3 * sine[floor(bassPhase * 2 % 1 * size)]) * exp(-inBeat * 3) * 0.22
            local step = floor(time / eighth) + 1
            local lead = plan.melody[step]
            if lead then
                local inEighth = time - (step - 1) * eighth
                local phase = time * lead % 1
                local triangle = phase < 0.5 and phase * 4 - 1 or 3 - phase * 4
                sample = sample + (triangle * 0.6 + sine[floor(phase * size)] * 0.4) * exp(-inEighth * style.decay) * 0.13 * style.lead
            end
            if style.drums > 0 then
                local whole = floor(beats) % 4
                if whole == 0 or whole == 2 then
                    sample = sample + sine[floor((120 * inBeat - 220 * inBeat * inBeat) % 1 * size)] * exp(-inBeat * 18) * 0.5 * style.drums
                else
                    noise = (noise * 1103515245 + 12345) % 2147483648
                    sample = sample + (noise / 1073741824 - 1) * exp(-inBeat * 22) * 0.16 * style.drums
                end
            end
            if style.hats > 0 then
                noise = (noise * 1103515245 + 12345) % 2147483648
                local inEighth = time - (step - 1) * eighth
                sample = sample + (noise / 1073741824 - 1) * exp(-inEighth * 70) * 0.045 * style.hats
            end
            local fade = math.min(1, time / 0.05, (plan.length - time) / 1.5)
            sample = sample / (1 + math.abs(sample)) * fade
            buffer[index] = floor(sample * 30000)
        end
        if count < Synth.chunk then
            parts[#parts + 1] = string.pack('<' .. string.rep('h', count), table.unpack(buffer, 1, count))
        else
            parts[#parts + 1] = string.pack(format, table.unpack(buffer, 1, count))
        end
        position = position + count
        jobs.checkpoint()
    end
    return table.concat(parts), plan.length
end

return Synth
