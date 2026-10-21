-- The faces of the people: SVG documents drawn from a few shapes in the style of the repository, a round face with dots for eyes, rosy cheeks and a smile under one of six haircuts, read with graphics.newVectorImage and named for the UI with ui.setImage, so every person has a face of a few hundred bytes that stays sharp at every size.
local graphics = require('haylen.graphics')
local ui = require('haylen.ui')

local Random = require('random')

local Avatars = {}

Avatars.outline = '#1E1B33'
Avatars.skins = {'#F8D7BE', '#EDBB94', '#D69C72', '#B97A53', '#8E5A3A', '#F3CBA5'}
Avatars.hairs = {'#2B1E1A', '#5A3825', '#9C4F2E', '#D9A441', '#3E3A52', '#C2410C', '#E9E4DA'}
Avatars.shirts = {'#2F6BFF', '#FF4F8B', '#22B07D', '#FFB547', '#8A5CFF', '#24B3C8', '#F06A3B', '#3E5A96'}
Avatars.backgrounds = {'#FFE3D3', '#DDEBFF', '#E2F5E9', '#FFF1C9', '#EEE3FF', '#D9F4F6', '#FFE0EA', '#E9ECF2'}

-- The haircuts, each a path drawn behind or over the face of 120 units.
Avatars.backHair = {
    long = 'M30 60 Q28 26 60 24 Q92 26 90 60 L92 96 Q76 100 70 92 L50 92 Q44 100 28 96 Z',
    bob = 'M31 58 Q30 27 60 26 Q90 27 89 58 L90 80 Q80 84 74 78 L46 78 Q40 84 30 80 Z',
}
Avatars.frontHair = {
    short = 'M34 54 Q33 28 60 28 Q87 28 86 54 Q78 42 64 40 Q50 46 34 54 Z',
    long = 'M34 56 Q35 30 60 30 Q85 30 86 56 Q74 46 66 38 Q56 50 34 56 Z',
    bob = 'M33 56 Q34 30 60 30 Q86 30 87 56 L80 48 Q64 44 52 40 Q44 50 33 56 Z',
    curly = 'M33 52 Q28 38 38 32 Q40 22 52 24 Q60 16 70 23 Q82 21 84 32 Q93 38 87 52 Q80 44 70 44 Q60 48 50 44 Q40 44 33 52 Z',
    bun = 'M34 54 Q33 30 60 29 Q87 30 86 54 Q76 42 60 41 Q44 42 34 54 Z',
    side = 'M34 56 Q31 30 58 28 Q86 27 87 50 Q72 46 60 38 Q52 50 34 56 Z',
}
Avatars.cuts = {'short', 'long', 'bob', 'curly', 'bun', 'side'}

-- Builds the document of a face from a seed. The face fills a circle, so it shows round as an icon too, and `ring` draws a dark ring around it.
function Avatars.document(seed, ring)
    local random = Random(seed)
    local skin, hair = random:pick(Avatars.skins), random:pick(Avatars.hairs)
    local shirt, background = random:pick(Avatars.shirts), random:pick(Avatars.backgrounds)
    local cut = Avatars.cuts[seed % #Avatars.cuts + 1]
    local glasses = random:chance(0.25)
    local o = Avatars.outline
    local parts = {}
    local function add(format, ...)
        parts[#parts + 1] = string.format(format, ...)
    end
    add('<circle cx="60" cy="60" r="60" fill="%s"/>', background)
    if Avatars.backHair[cut] then
        add('<path d="%s" fill="%s" stroke="%s" stroke-width="4" stroke-linejoin="round"/>', Avatars.backHair[cut], hair, o)
    end
    if cut == 'bun' then
        add('<circle cx="60" cy="22" r="12" fill="%s" stroke="%s" stroke-width="4"/>', hair, o)
    end
    -- The shoulders follow the circle at the bottom, so nothing of the drawing leaves it.
    add('<path d="M18.6 103 Q26 84 60 84 Q94 84 101.4 103 A60 60 0 0 1 18.6 103 Z" fill="%s" stroke="%s" stroke-width="4" stroke-linejoin="round"/>', shirt, o)
    add('<rect x="53" y="74" width="14" height="14" rx="5" fill="%s" stroke="%s" stroke-width="4"/>', skin, o)
    add('<circle cx="60" cy="56" r="25" fill="%s" stroke="%s" stroke-width="4"/>', skin, o)
    add('<path d="%s" fill="%s" stroke="%s" stroke-width="4" stroke-linejoin="round"/>', Avatars.frontHair[cut], hair, o)
    add('<circle cx="51" cy="59" r="3.2" fill="%s"/><circle cx="69" cy="59" r="3.2" fill="%s"/>', o, o)
    add('<circle cx="46" cy="66" r="4.2" fill="#FF8FA3" opacity="0.55"/><circle cx="74" cy="66" r="4.2" fill="#FF8FA3" opacity="0.55"/>')
    add('<path d="M54 67 Q60 72.5 66 67" fill="none" stroke="%s" stroke-width="3" stroke-linecap="round"/>', o)
    if glasses then
        add('<g fill="none" stroke="%s" stroke-width="2.6"><circle cx="51" cy="59" r="7"/><circle cx="69" cy="59" r="7"/><path d="M58 59h4"/></g>', o)
    end
    add('<path d="M38 40 Q44 32 53 30" fill="none" stroke="#FFFFFF" stroke-opacity="0.35" stroke-width="3.5" stroke-linecap="round"/>')
    if ring then
        add('<circle cx="60" cy="60" r="57.5" fill="none" stroke="%s" stroke-width="5"/>', o)
    end
    return '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 120 120" width="120" height="120">' .. table.concat(parts) .. '</svg>'
end

-- Names the face of a person for the UI and returns the name.
function Avatars.register(id, seed)
    local name = '@avatar-' .. id
    ui.setImage(name, graphics.newVectorImage(Avatars.document(seed)))
    return name
end

-- Names the face of the person who uses the app with a ring, for the tab of the profile while it shows.
function Avatars.registerRing(id, seed)
    local name = '@avatar-' .. id .. '-ring'
    ui.setImage(name, graphics.newVectorImage(Avatars.document(seed, true)))
    return name
end

return Avatars
